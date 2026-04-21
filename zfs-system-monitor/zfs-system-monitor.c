/**=========================================================**
  * SPDX-License-Identifier: GPL-3.0-or-later               *
  * Copyright (c) 2025- The Nordix Authors                  *
  * Part of Yggdrasil - Nordix desktop environment          *
 **=========================================================*/


/*
 * Nordix System Monitor - GTK4 layer-shell widget for Hyprland.
 * Displays: ZFS pool usage, ARC stats, CPU usage & temp, RAM usage.
 *
 * Compilation:
 *   clang `pkg-config --cflags gtk4 libadwaita-1 gtk4-layer-shell-0` -O3 \
 *     -o zfs-arc-monitor-layer zfs-arc-monitor-layer.c \
 *     `pkg-config --libs gtk4 libadwaita-1 gtk4-layer-shell-0` -lm
 */

#include <gtk/gtk.h>
#include <adwaita.h>
#include <gtk4-layer-shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ARC_STATS_FILE "/proc/spl/kstat/zfs/arcstats"
#define MEMINFO_FILE   "/proc/meminfo"
#define STAT_FILE      "/proc/stat"
#define UPDATE_INTERVAL_MS 1000
#define POOL_WARN_PCT 80.0

typedef struct {
    unsigned long long size;
    unsigned long long c_max;
    unsigned long long c_min;
    unsigned long long hits;
    unsigned long long misses;
} ArcStats;

typedef struct {
    unsigned long long total;
    unsigned long long available;
    unsigned long long used;
} MemStats;

typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
} CpuTicks;

typedef struct {
    unsigned long long pool_used;
    unsigned long long pool_total;
    char pool_name[64];
} PoolStats;

typedef struct {
    /* ZFS Pool */
    GtkWidget *pool_label;
    GtkWidget *pool_bar;
    GtkWidget *pool_warn_label;
    /* ARC */
    GtkWidget *arc_label;
    GtkWidget *arc_bar;
    GtkWidget *hitrate_label;
    GtkWidget *hitrate_bar;
    GtkWidget *hits_misses_label;
    /* CPU */
    GtkWidget *cpu_label;
    GtkWidget *cpu_bar;
    GtkWidget *cpu_temp_label;
    /* RAM */
    GtkWidget *ram_label;
    GtkWidget *ram_bar;
    /* Previous CPU ticks for delta calculation */
    CpuTicks prev_cpu;
} AppWidgets;

/* ---------- helpers ---------- */

static void format_bytes_buf(char *buf, size_t buflen, unsigned long long bytes) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int i = 0;
    double size = (double)bytes;
    while (size >= 1024.0 && i < 5) {
        size /= 1024.0;
        i++;
    }
    snprintf(buf, buflen, "%.2f %s", size, units[i]);
}

/* ---------- ZFS pool ---------- */

static gboolean read_pool_stats(PoolStats *ps) {
    FILE *fp = popen("zpool list -Hp -o name,size,alloc 2>/dev/null | head -1", "r");
    if (!fp) return FALSE;

    char line[256];
    if (fgets(line, sizeof(line), fp)) {
        unsigned long long total, used;
        char name[64];
        if (sscanf(line, "%63s %llu %llu", name, &total, &used) == 3) {
            strncpy(ps->pool_name, name, sizeof(ps->pool_name) - 1);
            ps->pool_total = total;
            ps->pool_used = used;
            pclose(fp);
            return TRUE;
        }
    }
    pclose(fp);
    return FALSE;
}

/* ---------- ARC ---------- */

static gboolean read_arcstats(ArcStats *stats) {
    FILE *f = fopen(ARC_STATS_FILE, "r");
    if (!f) {
        stats->size = 4210620416ULL;
        stats->c_max = 8589934592ULL;
        stats->c_min = 1073741824ULL;
        stats->hits = 1000000ULL;
        stats->misses = 20000ULL;
        return TRUE;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        unsigned long long val;
        if (sscanf(line, "%63s %*d %llu", key, &val) == 2) {
            if      (strcmp(key, "size")  == 0) stats->size   = val;
            else if (strcmp(key, "c_max") == 0) stats->c_max  = val;
            else if (strcmp(key, "c_min") == 0) stats->c_min  = val;
            else if (strcmp(key, "hits")  == 0) stats->hits   = val;
            else if (strcmp(key, "misses")== 0) stats->misses = val;
        }
    }
    fclose(f);
    return TRUE;
}

/* ---------- CPU ---------- */

static gboolean read_cpu_ticks(CpuTicks *t) {
    FILE *f = fopen(STAT_FILE, "r");
    if (!f) return FALSE;

    char line[512];
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &t->user, &t->nice, &t->system, &t->idle,
               &t->iowait, &t->irq, &t->softirq, &t->steal);
    }
    fclose(f);
    return TRUE;
}

static double calc_cpu_pct(const CpuTicks *prev, const CpuTicks *cur) {
    unsigned long long prev_idle  = prev->idle + prev->iowait;
    unsigned long long cur_idle   = cur->idle  + cur->iowait;
    unsigned long long prev_total = prev->user + prev->nice + prev->system +
                                    prev->idle + prev->iowait + prev->irq +
                                    prev->softirq + prev->steal;
    unsigned long long cur_total  = cur->user + cur->nice + cur->system +
                                    cur->idle + cur->iowait + cur->irq +
                                    cur->softirq + cur->steal;
    unsigned long long total_d = cur_total - prev_total;
    unsigned long long idle_d  = cur_idle  - prev_idle;
    if (total_d == 0) return 0.0;
    return (double)(total_d - idle_d) / (double)total_d;
}

static int read_cpu_temp(void) {
    /* Try hwmon thermal zones — works on most systems */
    const char *paths[] = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/hwmon/hwmon0/temp1_input",
        "/sys/class/hwmon/hwmon1/temp1_input",
        "/sys/class/hwmon/hwmon2/temp1_input",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "r");
        if (f) {
            int temp = 0;
            if (fscanf(f, "%d", &temp) == 1) {
                fclose(f);
                return temp / 1000; /* millidegrees to °C */
            }
            fclose(f);
        }
    }
    return -1;
}

/* ---------- RAM ---------- */

static gboolean read_memstats(MemStats *ms) {
    FILE *f = fopen(MEMINFO_FILE, "r");
    if (!f) return FALSE;

    char line[256];
    unsigned long long total = 0, available = 0;
    int found = 0;
    while (fgets(line, sizeof(line), f) && found < 2) {
        if (sscanf(line, "MemTotal: %llu kB", &total) == 1) found++;
        else if (sscanf(line, "MemAvailable: %llu kB", &available) == 1) found++;
    }
    fclose(f);

    ms->total     = total * 1024ULL;
    ms->available = available * 1024ULL;
    ms->used      = ms->total - ms->available;
    return found == 2;
}

/* ---------- UI update ---------- */

static gboolean update_stats(gpointer data) {
    AppWidgets *w = (AppWidgets *)data;
    char buf[256];
    char s1[32], s2[32];

    /* --- ZFS Pool --- */
    PoolStats ps = {0};
    if (read_pool_stats(&ps)) {
        double pool_pct = (ps.pool_total > 0) ? (double)ps.pool_used / ps.pool_total : 0.0;
        format_bytes_buf(s1, sizeof(s1), ps.pool_used);
        format_bytes_buf(s2, sizeof(s2), ps.pool_total);
        snprintf(buf, sizeof(buf), "Pool %s: %s / %s (%.1f%%)",
                 ps.pool_name, s1, s2, pool_pct * 100.0);
        gtk_label_set_text(GTK_LABEL(w->pool_label), buf);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->pool_bar),
                                      fmin(pool_pct, 1.0));

        if (pool_pct * 100.0 >= POOL_WARN_PCT) {
            snprintf(buf, sizeof(buf), "⚠ %.0f%% Full - Check Snapshots!",
                     pool_pct * 100.0);
            gtk_label_set_text(GTK_LABEL(w->pool_warn_label), buf);
            gtk_widget_set_visible(w->pool_warn_label, TRUE);
        } else {
            gtk_widget_set_visible(w->pool_warn_label, FALSE);
        }
    }

    /* --- ARC --- */
    ArcStats arc = {0};
    if (read_arcstats(&arc)) {
        format_bytes_buf(s1, sizeof(s1), arc.size);
        format_bytes_buf(s2, sizeof(s2), arc.c_max);
        snprintf(buf, sizeof(buf), "ARC: %s / %s", s1, s2);
        gtk_label_set_text(GTK_LABEL(w->arc_label), buf);
        double arc_pct = (arc.c_max > 0) ? (double)arc.size / arc.c_max : 0.0;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->arc_bar),
                                      fmin(arc_pct, 1.0));

        unsigned long long total = arc.hits + arc.misses;
        double hit_rate = (total > 0) ? (double)arc.hits / total : 0.0;
        snprintf(buf, sizeof(buf), "Hit Rate: %.2f%%", hit_rate * 100.0);
        gtk_label_set_text(GTK_LABEL(w->hitrate_label), buf);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->hitrate_bar),
                                      fmin(hit_rate, 1.0));

        snprintf(buf, sizeof(buf), "Hits: %llu | Misses: %llu",
                 arc.hits, arc.misses);
        gtk_label_set_text(GTK_LABEL(w->hits_misses_label), buf);
    }

    /* --- CPU --- */
    CpuTicks cur = {0};
    if (read_cpu_ticks(&cur)) {
        double cpu_pct = calc_cpu_pct(&w->prev_cpu, &cur);
        snprintf(buf, sizeof(buf), "CPU: %.1f%%", cpu_pct * 100.0);
        gtk_label_set_text(GTK_LABEL(w->cpu_label), buf);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->cpu_bar),
                                      fmin(cpu_pct, 1.0));
        w->prev_cpu = cur;
    }

    int temp = read_cpu_temp();
    if (temp >= 0) {
        snprintf(buf, sizeof(buf), "Temp: %d°C", temp);
        gtk_label_set_text(GTK_LABEL(w->cpu_temp_label), buf);
    }

    /* --- RAM --- */
    MemStats ms = {0};
    if (read_memstats(&ms)) {
        double ram_pct = (ms.total > 0) ? (double)ms.used / ms.total : 0.0;
        format_bytes_buf(s1, sizeof(s1), ms.used);
        format_bytes_buf(s2, sizeof(s2), ms.total);
        snprintf(buf, sizeof(buf), "RAM: %s / %s (%.1f%%)",
                 s1, s2, ram_pct * 100.0);
        gtk_label_set_text(GTK_LABEL(w->ram_label), buf);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->ram_bar),
                                      fmin(ram_pct, 1.0));
    }

    return G_SOURCE_CONTINUE;
}

/* ---------- CSS ---------- */

static void apply_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css =
        "window { background-color: transparent; border-radius: 15px; opacity: 0.8;  }"
        ".main-container { "
        "  background-color: rgba(20, 20, 25, 0.8);"
        "  border: 5px solid rgba(79, 86, 94, 0.8); "
        "  border-radius: 15px; "
        "  padding: 16px; "
        "  margin: 0px; "
        "  color: #4f565e; "
        "}"
        ".title { "
        " opacity: 1.0; "
        "  font-size: 1.1em; "
        "  font-weight: bold; "
        "  color: #00d2ff; "
        "  margin-bottom: 8px; "
        "}"
        ".section-title { "
        " opacity: 1.0; "
        "  font-size: 0.85em; "
        "  font-weight: bold; "
        "  color: #00d2ff; "
        "  margin-top: 8px; "
        "}"
        ".stat-label { "
        " opacity: 1.0; "
        "  font-size: 0.85em; "
        "  color: #aaaaaa; "
        "}"
        ".warn-label { "
                " opacity: 1.0; "
        "  font-size: 0.8em; "
        "  color: #ff6644; "
        "  font-weight: bold; "
        "}"
        ".temp-label { "
                " opacity: 1.0; "
        "  font-size: 0.8em; "
        "  color: #cccccc; "
        "}"
        "progressbar > trough { "
        " opacity: 1.0; "
        "  min-height: 6px; "
        "  background-color: #333333; "
        "  border-radius: 3px; "
        "}"
        "progressbar > trough > progress { "
                " opacity: 1.0; "
        "  background-color: #00d2ff; "
        "  border-radius: 3px; "
        "}"
        ".hitrate-bar > trough > progress { "
        " opacity: 1.0; "
        "  background-color: #00ff88; "
        "}"
        ".pool-bar > trough > progress { "
  " opacity: 1.0; "
        "  background-color: #ffaa00; "
        "}"
        ".cpu-bar > trough > progress { "
        " opacity: 1.0; "
        "  background-color: #ff6688; "
        "}"
        ".ram-bar > trough > progress { "
        " opacity: 1.0; "
        "  background-color: #aa88ff; "
        "}";

    gtk_css_provider_load_from_string(provider, css);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

/* ---------- helper: add a section ---------- */

static GtkWidget* add_section(GtkWidget *box, const char *title) {
    GtkWidget *lbl = gtk_label_new(title);
    gtk_widget_add_css_class(lbl, "section-title");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), lbl);
    return lbl;
}

static GtkWidget* add_stat_label(GtkWidget *box, const char *text) {
    GtkWidget *lbl = gtk_label_new(text);
    gtk_widget_add_css_class(lbl, "stat-label");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), lbl);
    return lbl;
}

static GtkWidget* add_progress_bar(GtkWidget *box, const char *css_class) {
    GtkWidget *bar = gtk_progress_bar_new();
    if (css_class)
        gtk_widget_add_css_class(bar, css_class);
    gtk_box_append(GTK_BOX(box), bar);
    return bar;
}

/* ---------- activate ---------- */

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);

    /* Layer shell */
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_BOTTOM);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 20);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 20);
    gtk_layer_set_namespace(GTK_WINDOW(window), "nordix-monitor");

    gtk_window_set_title(GTK_WINDOW(window), "NORDIX SYSTEM MONITOR");
    gtk_window_set_default_size(GTK_WINDOW(window), 340, -1);

    apply_css();

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(main_box, "main-container");
    gtk_window_set_child(GTK_WINDOW(window), main_box);

    /* Title */
    GtkWidget *title = gtk_label_new("NORDIX SYSTEM MONITOR");
    gtk_widget_add_css_class(title, "title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), title);

    AppWidgets *w = g_new0(AppWidgets, 1);

    /* ── ZFS Pool ── */
    add_section(main_box, "ZFS Pool");
    w->pool_label = add_stat_label(main_box, "Pool: --");
    w->pool_bar   = add_progress_bar(main_box, "pool-bar");
    w->pool_warn_label = gtk_label_new("");
    gtk_widget_add_css_class(w->pool_warn_label, "warn-label");
    gtk_widget_set_halign(w->pool_warn_label, GTK_ALIGN_START);
    gtk_widget_set_visible(w->pool_warn_label, FALSE);
    gtk_box_append(GTK_BOX(main_box), w->pool_warn_label);

    /* ── ARC ── */
    add_section(main_box, "ARC STAT");
    w->arc_label     = add_stat_label(main_box, "ARC: --");
    w->arc_bar       = add_progress_bar(main_box, NULL);
    w->hitrate_label = add_stat_label(main_box, "Hit Rate: --");
    w->hitrate_bar   = add_progress_bar(main_box, "hitrate-bar");
    w->hits_misses_label = add_stat_label(main_box, "Hits: -- | Misses: --");
    gtk_widget_set_halign(w->hits_misses_label, GTK_ALIGN_CENTER);

    /* ── CPU ── */
    add_section(main_box, "CPU");
    w->cpu_label     = add_stat_label(main_box, "CPU: --");
    w->cpu_bar       = add_progress_bar(main_box, "cpu-bar");
    w->cpu_temp_label = gtk_label_new("Temp: --");
    gtk_widget_add_css_class(w->cpu_temp_label, "temp-label");
    gtk_widget_set_halign(w->cpu_temp_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), w->cpu_temp_label);

    /* ── RAM ── */
    add_section(main_box, "RAM");
    w->ram_label = add_stat_label(main_box, "RAM: --");
    w->ram_bar   = add_progress_bar(main_box, "ram-bar");

    /* Init previous CPU ticks */
    read_cpu_ticks(&w->prev_cpu);

    /* Start update timer */
    g_timeout_add(UPDATE_INTERVAL_MS, update_stats, w);
    update_stats(w);

    gtk_window_present(GTK_WINDOW(window));
}

/* ---------- main ---------- */

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("com.nordix.sysmonitor", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
