/**=========================================================**
  * SPDX-License-Identifier: GPL-3.0-or-later               *
  * Copyright (c) 2025- The Nordix Authors                  *
  * Part of Yggdrasil - Nordix desktop environment          *
 **=========================================================*/
/*
 * ZFS ARC Monitor - A minimal GTK4 widget for monitoring ZFS ARC stats.
 * Displays: size, c_max, c_min, hits, misses.
 *
 * Compilation: gcc `pkg-config --cflags gtk4 libadwaita-1` -o zfs-arc-monitor zfs-arc-monitor.c `pkg-config --libs gtk4 libadwaita-1` -lm
 */

#include <gtk/gtk.h>
#include <adwaita.h>
#include <gtk4-layer-shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ARC_STATS_FILE "/proc/spl/kstat/zfs/arcstats"
#define UPDATE_INTERVAL_MS 1000

typedef struct {
    unsigned long long size;
    unsigned long long c_max;
    unsigned long long c_min;
    unsigned long long hits;
    unsigned long long misses;
} ArcStats;

typedef struct {
    GtkWidget *usage_label;
    GtkWidget *usage_bar;
    GtkWidget *hitrate_label;
    GtkWidget *hitrate_bar;
    GtkWidget *hits_misses_label;
} AppWidgets;

// Helper to format bytes into human-readable strings
static char* format_bytes(unsigned long long bytes) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int i = 0;
    double size = (double)bytes;

    while (size >= 1024 && i < 5) {
        size /= 1024;
        i++;
    }

    char *buf = malloc(32);
    snprintf(buf, 32, "%.2f %s", size, units[i]);
    return buf;
}

// Read ARC stats from /proc/spl/kstat/zfs/arcstats
static gboolean read_arcstats(ArcStats *stats) {
    FILE *f = fopen(ARC_STATS_FILE, "r");
    if (!f) {
        // Fallback for testing on systems without ZFS
        stats->size = 4210620416ULL;
        stats->c_max = 8589934592ULL;
        stats->c_min = 1073741824ULL;
        stats->hits = 1000000ULL;
        stats->misses = 20000ULL;
        return TRUE;
    }

    char line[256];
    int found_count = 0;

    while (fgets(line, sizeof(line), f)) {
        char key[64];
        unsigned long long val;
        // The format is typically: key type value
        if (sscanf(line, "%63s %*d %llu", key, &val) == 2) {
            if (strcmp(key, "size") == 0) { stats->size = val; found_count++; }
            else if (strcmp(key, "c_max") == 0) { stats->c_max = val; found_count++; }
            else if (strcmp(key, "c_min") == 0) { stats->c_min = val; found_count++; }
            else if (strcmp(key, "hits") == 0) { stats->hits = val; found_count++; }
            else if (strcmp(key, "misses") == 0) { stats->misses = val; found_count++; }
        }
    }

    fclose(f);
    return found_count >= 1;
}

// Update the UI with new stats
static gboolean update_stats(gpointer data) {
    AppWidgets *widgets = (AppWidgets *)data;
    ArcStats stats = {0};

    if (read_arcstats(&stats)) {
        // ARC Size
        char *size_str = format_bytes(stats.size);
        char *max_str = format_bytes(stats.c_max);
        char label_buf[128];
        snprintf(label_buf, sizeof(label_buf), "ARC Size: %s / %s", size_str, max_str);
        gtk_label_set_text(GTK_LABEL(widgets->usage_label), label_buf);
        free(size_str);
        free(max_str);

        double usage_pct = (stats.c_max > 0) ? (double)stats.size / stats.c_max : 0.0;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widgets->usage_bar), (usage_pct > 1.0) ? 1.0 : usage_pct);

        // Hit Rate
        unsigned long long total = stats.hits + stats.misses;
        double hit_rate = (total > 0) ? (double)stats.hits / total : 0.0;
        snprintf(label_buf, sizeof(label_buf), "Hit Rate: %.2f%%", hit_rate * 100.0);
        gtk_label_set_text(GTK_LABEL(widgets->hitrate_label), label_buf);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widgets->hitrate_bar), (hit_rate > 1.0) ? 1.0 : hit_rate);

        // Hits/Misses
        snprintf(label_buf, sizeof(label_buf), "Hits: %llu | Misses: %llu", stats.hits, stats.misses);
        gtk_label_set_text(GTK_LABEL(widgets->hits_misses_label), label_buf);
    }

    return G_SOURCE_CONTINUE;
}

// Setup CSS styles
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
        ".stat-label { "
        "  font-size: 0.9em; "
        "  color: #aaaaaa; "
        "}"
        "progress { "
        "  min-height: 6px; "
        "  background-color: #333333; "
        "  border-radius: 3px; "
        "}"
        "progressbar > trough > progress { "
        "  background-color: #00d2ff; "
        "  border-radius: 3px; "
        "}"
        ".hitrate-progress progressbar > trough > progress { "
        "  background-color: #00ff88; "
        "}";

    gtk_css_provider_load_from_string(provider, css);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_BOTTOM);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 20);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 20);
    gtk_layer_set_namespace(GTK_WINDOW(window), "zfs-arc-monitor");
    gtk_window_set_title(GTK_WINDOW(window), "NORDIX - ZFS ARC STATS");
    gtk_window_set_default_size(GTK_WINDOW(window), 320, 200);

    // Apply CSS
    apply_css();

    // Main layout
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(main_box, "main-container");
    gtk_window_set_child(GTK_WINDOW(window), main_box);

    // Title
    GtkWidget *title_label = gtk_label_new("NORDIX - ZFS ARC STATS");
    gtk_widget_add_css_class(title_label, "title");
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), title_label);

    // ARC Usage Section
    AppWidgets *widgets = g_new0(AppWidgets, 1);
    
    widgets->usage_label = gtk_label_new("ARC Size: --");
    gtk_widget_add_css_class(widgets->usage_label, "stat-label");
    gtk_widget_set_halign(widgets->usage_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), widgets->usage_label);

    widgets->usage_bar = gtk_progress_bar_new();
    gtk_box_append(GTK_BOX(main_box), widgets->usage_bar);

    // Hit Rate Section
    widgets->hitrate_label = gtk_label_new("Hit Rate: --");
    gtk_widget_add_css_class(widgets->hitrate_label, "stat-label");
    gtk_widget_set_halign(widgets->hitrate_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), widgets->hitrate_label);

    widgets->hitrate_bar = gtk_progress_bar_new();
    gtk_widget_add_css_class(widgets->hitrate_bar, "hitrate-progress");
    gtk_box_append(GTK_BOX(main_box), widgets->hitrate_bar);

    // Hits/Misses
    widgets->hits_misses_label = gtk_label_new("Hits: -- | Misses: --");
    gtk_widget_add_css_class(widgets->hits_misses_label, "stat-label");
    gtk_widget_set_halign(widgets->hits_misses_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(main_box), widgets->hits_misses_label);

    // Start timer
    g_timeout_add(UPDATE_INTERVAL_MS, update_stats, widgets);
    update_stats(widgets);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("com.nordix.zfsmonitor-layer", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
