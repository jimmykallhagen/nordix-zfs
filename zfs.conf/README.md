# Nordix ZFS Kernel Module Configuration

**File:** `/etc/modprobe.d/zfs.conf`
**Part of:** [Nordix](https://github.com/jimmykallhagen/Nordix)
**Requires:** OpenZFS 2.2.0 or newer (tested with 2.4.1)

---

## Overview

These are pre-tuned ZFS kernel module parameter files (`/etc/modprobe.d/zfs.conf`) for the Nordix Linux distribution, optimized for **desktop and gaming performance** on NVMe/SSD storage.

ZFS defaults are designed for enterprise servers with spinning disks and conservative memory usage. These configurations shift the balance toward interactive desktop responsiveness: aggressive ARC caching, high I/O parallelism, fast scrub completion, and SSD-aware allocation — while remaining stable and safe for daily use.

Each file is a complete, drop-in replacement. Pick the one that matches your system RAM and copy it to `/etc/modprobe.d/zfs.conf`.

---

## Available Profiles

| File             | System RAM | ARC Max | ARC Min | Effective Cache* | Aggressiveness |
|------------------|------------|---------|---------|------------------|----------------|
| `zfs-8gb.conf`   | 8 GB       | 4 GiB   | 1 GiB   | ~8–12 GiB        | Conservative   |
| `zfs-16gb.conf`  | 16 GB      | 8 GiB   | 2 GiB   | ~16–24 GiB       | Balanced       |
| `zfs-32gb.conf`  | 32 GB      | 20 GiB  | 8 GiB   | ~50–60 GiB       | Performance    |
| `zfs-64gb.conf`  | 64 GB      | 40 GiB  | 15 GiB  | ~100–120 GiB     | Aggressive     |
| `zfs-128gb.conf` | 128 GB     | 80 GiB  | 30 GiB  | ~200–240 GiB     | Extreme        |

*\* Effective cache assumes compressed ARC enabled with ~2.5x ZSTD compression ratio.*

## Installation

```bash
# 1. Copy the appropriate file for your RAM size
sudo cp zfs-32gb.conf /etc/modprobe.d/zfs.conf

# 2. If ZFS is in your mkinitcpio MODULES=() (required for ZFS-on-root):
sudo mkinitcpio -P

# 3. Reboot
sudo reboot
```

> **Important:** If `zfs` is listed in `MODULES=()` in `/etc/mkinitcpio.conf` (which it must be for ZFS-on-root), you **must** run `mkinitcpio -P` before rebooting. Otherwise your changes are silently ignored because the old parameters are baked into the initramfs image.

---

## What Gets Tuned (22 Parameters)

### ARC Memory (5 parameters)

The Adaptive Replacement Cache is ZFS's in-memory read cache. Unlike a simple LRU cache, the ARC uses a patented algorithm that balances recency and frequency to maximize hit rates. On a well-tuned desktop system, 90–98% of all disk reads are served from RAM.

| Parameter                   | What it does                              | How it scales            |
|-----------------------------|-------------------------------------------|--------------------------|
| `zfs_arc_max`               | Upper bound of cache size                 | 50–62% of RAM            |
| `zfs_arc_min`               | Floor that survives memory pressure       | 25–37% of arc_max        |
| `zfs_arc_lotsfree_percent`  | When to start yielding memory             | 8% (8 GB) → 1% (64+ GB)  |
| `zfs_arc_meta_balance`      | Adaptive metadata vs data eviction weight | 500 (kernel default)     |
| `zfs_arc_average_blocksize` | Hash table sizing hint                    | 64 KiB                   |

> **Note on OpenZFS 2.2.0 changes:** The legacy parameters `zfs_arc_meta_limit_percent`, `zfs_arc_meta_prune`, and `zfs_arc_meta_adjust_behavior` were removed upstream and replaced by the single adaptive `zfs_arc_meta_balance` knob. The new algorithm tracks ghost-hit counts for evicted metadata and data separately and adjusts eviction pressure based on actual access patterns rather than a fixed policy.

### Compressed ARC (1 parameter)

Stores cached data in compressed form, effectively multiplying ARC capacity by the compression ratio. A 64 GB system with 40 GiB ARC behaves like it has 100+ GiB of cache. CPU overhead is negligible on modern hardware.

### I/O Queue Depths (8 parameters)

NVMe drives support tens of thousands of concurrent commands across multiple hardware queues. ZFS defaults are tuned for HDDs with queue depths of 1–10. These profiles increase parallelism proportionally to system RAM (which correlates with expected workload intensity).

| Parameter       | 8 GB | 16 GB | 32 GB | 64 GB | 128 GB |
|-----------------|------|-------|-------|-------|--------|
| Async read max  | 16   | 24    | 32    | 32    | 64     |
| Sync read max   | 16   | 24    | 32    | 32    | 64     |
| Async write max | 10   | 12    | 16    | 16    | 24     |
| Sync write max  | 5    | 6     | 8     | 8     | 10     |

### Write Management (3 parameters)

| Parameter                    | What it does                      | How it scales                    |
|------------------------------|-----------------------------------|----------------------------------|
| `zfs_dirty_data_max_percent` | RAM available for write buffering | 15% (8 GB) → 40% (64+ GB)        |
| `zfs_txg_timeout`            | Flush interval                    | 5 sec (all profiles)             |
| `zfs_immediate_write_sz`     | ZIL bypass threshold              | 64 KiB (8 GB) → 128 KiB (16+ GB) |

### Scrub & Resilver (1 parameter)

Scrub I/O depth (`zfs_vdev_scrub_max_active`) scales from 3 (8 GB) to 8 (128 GB) for fast completion on NVMe/SSD.

> **Note on OpenZFS 2.2.0 changes:** The old time-based throttles (`zfs_scrub_delay`, `zfs_resilver_delay`, `zfs_scan_idle`) were removed and replaced by the NIA (Non-Interactive I/O) scheduler, which promotes scrub to `zfs_vdev_scrub_max_active` when the vdev is idle and drops it to `zfs_vdev_scrub_min_active` under interactive load. This adapts automatically without tick-based delays. The defaults for `zfs_vdev_nia_delay=5` and `zfs_vdev_nia_credit=5` are left untouched.

### I/O Aggregation (3 parameters)

Controls how aggressively ZFS merges adjacent I/O requests into single operations. Larger gap limits allow merging reads/writes that are close but not perfectly contiguous on disk.

| Parameter         | 8 GB    | 16 GB   | 32+ GB  |
|-------------------|---------|---------|---------|
| Aggregation limit | 128 KiB | 128 KiB | 128 KiB |
| Read gap limit    | 32 KiB  | 48 KiB  | 64 KiB  |
| Write gap limit   | 8 KiB   | 12 KiB  | 16 KiB  |

### SSD Optimization (1 parameter)

`metaslab_lba_weighting_enabled=0` disables the HDD-oriented allocation bias that prefers lower LBA addresses (outer platter edge on spinning disks). On NVMe/SSD, all addresses are equal - disabling this gives more even wear and reduced fragmentation.

---

## Scaling Philosophy

The profiles are not just the same file with different ARC sizes. As system RAM increases:

- **Memory pressure thresholds decrease** - more RAM means the system can absorb sudden spikes without OOM risk, so the ARC can hold onto data more aggressively.
- **I/O parallelism increases** - larger systems tend to have higher-end NVMe devices and heavier workloads that benefit from deeper command queues.
- **Write buffers grow** - more RAM means more space for dirty data accumulation, enabling better write coalescing and fewer TXG commits.
- **L2ARC recommendations change** - from "strongly recommended" at 8 GB to "unnecessary" at 128 GB.

---

## L2ARC Guidance

All profiles include commented-out L2ARC parameters. Uncomment them if you add a dedicated cache device (`zpool add <pool> cache <device>`).

| System RAM | L2ARC Recommendation                                                          |
|------------|-------------------------------------------------------------------------------|
| 8 GB       | **Strongly recommended** — 4 GiB ARC cannot hold a modern desktop working set |
| 16 GB      | **Recommended** — meaningful benefit for heavy workloads                      |
| 32 GB      | **Optional** — 20 GiB ARC covers most desktop workloads                       |
| 64 GB      | **Usually unnecessary** — 100+ GiB effective capacity                         |
| 128 GB     | **Unnecessary** — 200+ GiB effective capacity exceeds any desktop working set |

---

## Verification

After rebooting with the new configuration:

```bash
# Verify parameters are active
cat /sys/module/zfs/parameters/zfs_arc_max
cat /sys/module/zfs/parameters/zfs_arc_min
cat /sys/module/zfs/parameters/zfs_arc_meta_balance

# Confirm no deprecated parameters are being rejected
sudo dmesg | grep "unknown parameter"   # should produce no output

# Check ARC hit rate (target: >90%)
awk '/^hits/{h=$3} /^misses/{m=$3} END{
  printf "%.1f%%\n",h/(h+m)*100}' /proc/spl/kstat/zfs/arcstats

# Full ARC summary
arc_summary

# Monitor ARC in real time
watch -n1 'grep -E "^(size|c_max|c_min|hits|misses)" /proc/spl/kstat/zfs/arcstats'

# Pool health
zpool status -x

# I/O monitoring
zpool iostat -v <pool> 1
```

---

## For HDD Pools

These profiles are optimized for NVMe/SSD. If your pool contains spinning disks, consider these adjustments (note: all parameters below exist in OpenZFS 2.2.0+):

- `zfs_vdev_scrub_max_active=2` - lower scrub parallelism so random-I/O scrubs don't thrash the heads
- `zfs_vdev_scrub_min_active=1` - minimum scrub I/Os under interactive load
- `zfs_vdev_nia_delay=10` - wait longer before considering the vdev idle (gives interactive I/O more priority)
- `zfs_vdev_nia_credit=2` - fewer non-interactive I/Os allowed while interactive is pending
- `zfs_vdev_async_read_max_active=10` - reduce read parallelism to prevent head thrashing
- `zfs_vdev_aggregation_limit=1048576` - larger merges to minimize seeking
- `metaslab_lba_weighting_enabled=1` - re-enable LBA bias (outer tracks are faster on HDDs)

The old `zfs_resilver_delay`, `zfs_scrub_delay`, and `zfs_scan_idle` parameters no longer exist - the NIA scheduler replaces them with the queue-depth-based approach shown above.

---

## File Documentation

Every parameter in every file includes inline documentation with:

- Default value, valid range, and Nordix setting
- Detailed explanation of what the parameter does and why
- Scaling guidance for different system sizes
- Warnings about potential risks

The files are designed to be self-documenting - you should be able to understand and customize any parameter just by reading the comments.

---

## Contributing

Contributions are welcome. By submitting a pull request, you agree that your contribution is licensed under GPL-3.0-or-later (the same license as the rest of the project) and you will be added to the `AUTHORS` file as a co-holder of copyright on your contribution.

Before opening a PR, please:

- Verify the configuration boots cleanly and no parameters are rejected (`dmesg | grep "unknown parameter"` should be empty)
- Test ARC behavior under typical desktop load and include `arc_summary` output in the PR description if you're changing tuning values
- Keep the inline documentation style consistent with existing parameters

---

## License

Nordix is licensed under the **GNU General Public License, version 3 or later**
(`SPDX-License-Identifier: GPL-3.0-or-later`).

Copyright (c) 2025 The Nordix Authors.

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See the [`LICENSE`](LICENSE) file or <https://www.gnu.org/licenses/gpl-3.0.html> for the full license text.
