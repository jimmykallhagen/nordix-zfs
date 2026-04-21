# Nordix ZFS Dataset Configuration

**Part of:** [Nordix](https://github.com/jimmykallhagen/Nordix)  
**License:** PolyForm-Noncommercial-1.0.0  
**Author:** Jimmy Källhagen

---

## **Nordix principles - Honor the developers** 
* **All credit should go to the developers**
* Nordix is ​​built on OpenZFS
* OpenZFS is currently the biggest contributor to ZFS development.

You can find their projects here:
- [**OpenZFS**](https://openzfs.org/wiki/Main_Page)

---

## Overview

This document describes the Nordix ZFS dataset layout — a carefully designed hierarchy that separates system, user, and application data into purpose-optimized datasets. This architecture enables **surgical rollbacks**, **workload-specific tuning**, and **efficient snapshot management**.

---

## Why Separate Datasets?

Traditional filesystems treat all data equally. ZFS datasets allow you to:

### 1. Granular Rollbacks
When you rollback a dataset, **only that dataset is affected**. By separating `/home` from `~/.config`, you can:
- Rollback your desktop configuration without losing downloaded files
- Restore your system root without affecting application data in `/var/lib`
- Undo game settings changes without re-downloading 100 GB of game files

### 2. Workload-Specific Optimization
Different data has different access patterns:

| Data Type | Access Pattern | Optimal Settings |
|-----------|---------------|------------------|
| Game files | Large sequential reads | `recordsize=1M`, `primarycache=all` |
| Logs | Append-only writes | `sync=disabled`, `compression=zstd-10` |
| Temp files | Ephemeral, high churn | `checksum=off`, `compression=lz4` |
| Documents | Small random I/O | `recordsize=32K`, `copies=2` |

### 3. Snapshot Efficiency
Datasets that change frequently (caches, logs, temp) are excluded from snapshots, which:
- Reduces snapshot size dramatically
- Speeds up snapshot creation and deletion
- Prevents rollbacks from restoring stale cache data

### 4. Security Isolation
Each dataset can have its own security properties:
- `exec=off` on data directories (Pictures, Music, Downloads)
- `setuid=off` to prevent privilege escalation
- `devices=off` to block device file creation

---

## Pool Configuration

Depending on what you chose during installation ashift=12 or ashift=13
```bash
zpool create -f \
  -o ashift=12 \
  -o autotrim=on \
  -o feature@large_dnode=enabled \
  -O redundant_metadata=most \
  -O dnodesize=auto \
  nordix "${ROOT_DISK}"
```

| Property | Value | Purpose |
|----------|-------|---------|
| `ashift=12` | 4K sectors | Optimal for standard: NVMe, SATA SSDs and HDD |
| `ashift=13` | 8K sectors | Optimal for some top model NVMe SSDs |
| `autotrim=on` | Automatic TRIM | Maintains SSD performance and longevity |
| `feature@large_dnode=enabled` | Pool feature | Required for dnodesize=auto |
| `dnodesize=auto` | Variable metadata | Efficient extended attribute storage |
| `redundant_metadata=most` | Extra metadata copies | Protects all metadata blocks with redundant copies  |

**NVMe** - lies 
If you check your system's drive data, you will often see that your NVME has a block size of 512 (ashift=9).
It is for compatibility. Please check the manufacturer's specifications for correct information.

### Metadata Redundancy
     
**redundant_metadata=all|most|some|none**<br>
Controls what types of metadata are stored redundantly. ZFS stores an extra copy of metadata, so that if a single block is corrupted, the amount of user data lost is limited. This extra copy is in addition to any redundancy provided at the pool level (e.g. by mirroring or RAID-Z), and is in addition to an extra copy specified by the copies property (up to a total of 3 copies). For example if the pool is mirrored, copies=2, and redundant_metadata=most, then ZFS stores 6 copies of most metadata, and 4 copies of data and some metadata.
When set to all, ZFS stores an extra copy of all metadata. If a single on-disk block is corrupt, at worst a single block of user data (which is recordsize bytes long) can be lost.
When set to most, ZFS stores an extra copy of most types of metadata. This can improve performance of random writes, because less metadata must be written. In practice, at worst about 1000 blocks (of recordsize bytes each) of user data can be lost if a single on-disk block is corrupt. The exact behavior of which metadata blocks are stored redundantly may change in future releases.
When set to some, ZFS stores an extra copy of only critical metadata. This can improve file create performance since less metadata needs to be written. If a single on-disk block is corrupt, multiple user files or directories can be lost.
When set to none, ZFS does not store any copies of metadata redundantly. If a single on-disk block is corrupt, an entire dataset can be lost.

The default value is all.
- **Source**: [**OpenZFS**](https://openzfs.github.io/openzfs-docs/man/v2.3/7/zfsprops.7.html#redundant_metadata)

### Dnode Size

**dnodesize=legacy|auto|1k|2k|4k|8k|16k**

Controls the size of dnodes (metadata nodes) in the filesystem. Default is `legacy` (512 bytes).

Nordix uses `dnodesize=auto` which allows ZFS to automatically choose the optimal dnode size. This is especially beneficial when combined with `xattr=sa`, as extended attributes are stored directly in the dnode's bonus buffer.

**Benefits of `dnodesize=auto`:**
- Faster access to extended attributes
- Fewer disk I/O operations for metadata
- Better performance for SELinux, Samba, and xattr-heavy applications

**Requirement:** The `large_dnode` pool feature must be enabled.

**Compatibility warning:** Datasets with large dnodes cannot be sent (`zfs send`) to pools without the `large_dnode` feature.
---

## Dataset Hierarchy

```
╔══════════════════════════════════════════════════════════════════════════════════════════╗
║                                   Nordix ZFS - Dataset                                   ║
║ nordix (zpool)                                                                           ║
║ ├── ROOT                                                                                 ║
║ │   └── default           ━   root (/)                         ━   [AUTO-SNAPSHOTS: YES] ║
║ ├── varcache              ━   /var/cache                       ━   [AUTO-SNAPSHOTS:  NO] ║
║ ├── varlog                ━   /var/log                         ━   [AUTO-SNAPSHOTS:  NO] ║
║ ├── varlib                ━   /var/lib                         ━   [AUTO-SNAPSHOTS: YES] ║
║ ├── vartmp                ━   /var/tmp                         ━   [AUTO-SNAPSHOTS:  NO] ║
║ ├── tmp                   ━   /tmp                             ━   [AUTO-SNAPSHOTS:  NO] ║
║ ├── opt                   ━   /opt                             ━   [AUTO-SNAPSHOTS: YES] ║
║ └── home                  ━   /home                            ━   [AUTO-SNAPSHOTS: YES] ║
║   ├── cache               ━   ~/.cache                         ━   [AUTO-SNAPSHOTS:  NO] ║
║   ├── config              ━   ~/.config                        ━   [AUTO-SNAPSHOTS: YES] ║
║   ├── local               ━   ~/.local                         ━   [AUTO-SNAPSHOTS: YES] ║
║   │  ├── lutris           ━   - Lutris runtime                 ━   [AUTO-SNAPSHOTS: YES] ║
║   │  └── steam            ━   - Steam root                     ━   [AUTO-SNAPSHOTS: YES] ║
║   │     ├── game          ━   - Steam/steamapps/common         ━   [AUTO-SNAPSHOTS:  NO] ║
║   │     ├── proton        ━   - Steam/compatibilitytools.d/    ━   [AUTO-SNAPSHOTS: YES] ║
║   │     └── shadercache   ━   - Steam/steamapps/shadercache/   ━   [AUTO-SNAPSHOTS:  NO] ║
║   ├── games               ━   ~/Games                          ━   [AUTO-SNAPSHOTS:  NO] ║
║   ├── wine-prefix         ━   ~/Wine-prefix                    ━   [AUTO-SNAPSHOTS: YES] ║
║   ├── documents           ━   ~/Documents                      ━   [AUTO-SNAPSHOTS: YES] ║
║   ├── pictures            ━   ~/Pictures                       ━   [AUTO-SNAPSHOTS: YES] ║
║   ├── videos              ━   ~/Videos                         ━   [AUTO-SNAPSHOTS: YES] ║
║   ├── music               ━   ~/Music                          ━   [AUTO-SNAPSHOTS: YES] ║
║   └── downloads           ━   ~/Download                       ━   [AUTO-SNAPSHOTS:  NO] ║
╚══════════════════════════════════════════════════════════════════════════════════════════╝
```

---

## Dataset Details

### System Root (`nordix/ROOT/default`)

The bootable system root with maximum data integrity and hardening.

```bash
zfs create -o mountpoint=/ \
  -o compression=zstd-5 \
  -o recordsize=128K \
  -o copies=2 \
  -o checksum=fletcher4 \
  -o xattr=sa \
  -o acltype=posixacl \
  nordix/ROOT/default
```

| Property | Value | Rationale |
|----------|-------|-----------|
| `compression=zstd-5` | Balanced | Good ratio without high CPU cost |
| `recordsize=128K` | Default | Mixed workload (binaries, configs, libs) |
| `copies=2` | Redundancy + Performance | See below |
| `xattr=sa` | System attributes | Store xattrs in dnodes, not hidden files |
| `acltype=posixacl` | POSIX ACLs | Required for systemd and modern Linux |

#### ZFS Hardening: `copies=2`

The `copies=2` property on the root dataset is a key hardening measure that provides **both redundancy and performance**:

**Redundancy Benefits:**
- ZFS stores two independent copies of every data block on disk
- If one copy becomes corrupted (bit rot, failed write, cosmic ray), ZFS automatically uses the good copy
- Combined with `redundant_metadata=most`, this gives near-RAID1 protection on a single disk
- Protects against silent data corruption that would go undetected on traditional filesystems

**Performance Benefits:**
- ZFS can read from **either copy** — the scheduler picks whichever is faster to access
- On HDDs: If one copy is closer to the current head position, ZFS reads that one
- On SSDs: Parallel reads from different NAND cells can improve throughput
- Read-heavy workloads (boot, application loading) benefit most

**Space Trade-off:**
- System root is typically 15-30 GB
- With `copies=2`, this becomes 20-50 GB on disk
- On modern 500 GB+ SSDs, this is an acceptable trade-off for critical system data
- User data datasets use `copies=1` to avoid doubling storage for large files

**Why only on root?**
- Root contains irreplaceable system files (binaries, libraries, kernel)
- User data (documents, media) can often be recovered from backups
- Game files can be re-downloaded
- Applying `copies=2` everywhere would almost double total storage usage

**Snapshot behavior:** Included in auto-snapshots. Rollback restores entire system state.

---

### Cache Directories (No Snapshots)

These datasets are **excluded from snapshots** because their contents are:
- Regeneratable (can be rebuilt)
- High-churn (change constantly)
- Not valuable after rollback (stale data)

#### `/var/cache`
```bash
-o compression=zstd-1    # Light compression (high throughput)
-o recordsize=32K        # Package manager chunk size
-o sync=standard         # Durability for pacman database
-o logbias=throughput    # Optimize for streaming writes
-o exec=off              # No execution from cache
```

#### `/var/log`
```bash
-o compression=zstd-10   # Maximum compression (logs compress extremely well)
-o sync=disabled         # Async writes (logs are not critical)
-o primarycache=metadata # Don't cache log content in ARC
```

#### `/tmp` and `/var/tmp`
```bash
-o compression=lz4       # Fastest compression
-o checksum=off          # No integrity checks (ephemeral data)
-o sync=disabled         # No write barriers
-o primarycache=metadata # Minimal caching
```

**Why exclude from snapshots?**
- Rolling back `/tmp` makes no sense — it's meant to be empty
- Restoring old logs creates confusion (missing recent entries)
- Cache data becomes invalid after system changes

---

### Application Data (`/var/lib`)

Persistent application state that survives system rollbacks.

```bash
-o compression=zstd-5
-o recordsize=16K        # Small records for databases
-o sync=standard         # Data integrity
-o logbias=latency       # Responsive database operations
```

**Snapshot behavior:** Separate snapshots. Database state preserved independently of system.

**Use case:** Rollback `/` to yesterday, but keep today's PostgreSQL data.

---

### Home Directory Structure

The home directory is split into multiple datasets for maximum rollback flexibility.

#### User Home (`nordix/home`)
Base home directory with general user files.

```bash
-o compression=zstd-1
-o recordsize=64K
-o copies=1              # Single copy (important data has own datasets)
```

#### User Config (`~/.config`)
Desktop environment and application settings.

```bash
-o compression=zstd-5
-o recordsize=32K
-o exec=off              # Configs should not be executable
```

**Snapshot behavior:** Rollback broken configs without losing documents.

#### User Cache (`~/.cache`)
Regeneratable cache data (thumbnails, browser cache, build artifacts).

```bash
-o compression=zstd-5
-o recordsize=16K
-o exec=off
```

**Snapshot behavior:** Excluded. Cache rebuilds automatically.

---

### Media Directories

Optimized for large, already-compressed files (images, video, audio).

#### Pictures
```bash
-o compression=lz4       # Fast (JPG/PNG already compressed)
-o recordsize=1M         # Large sequential reads
-o primarycache=metadata # Don't waste RAM caching image data
-o secondarycache=none   # Don't use L2ARC for media
```

#### Videos
```bash
-o recordsize=4M         # Very large sequential reads
-o primarycache=metadata
```

#### Music
```bash
-o recordsize=2M         # Album-sized chunks
-o primarycache=metadata
```

**Why minimal caching?**
Media files are typically read once (playback) and don't benefit from caching. Saving ARC space for frequently-accessed system data improves overall performance.

---

### Gaming Datasets

Purpose-built for Steam, Lutris, and Wine gaming.

#### Game Installations (`nordix/home/local/steam/game`)
```bash
-o compression=zstd-4    # Moderate compression
-o recordsize=1M         # Large game assets
-o primarycache=all      # Cache frequently-loaded textures
```

**Snapshot behavior:** Excluded. Games are 50-150 GB each — snapshots would explode storage. Re-download if needed.

#### Proton/Wine Compatibility Tools
```bash
-o compression=zstd-5
-o recordsize=32K
```

**Snapshot behavior:** Rollback broken Proton versions.

#### Shader Cache
```bash
-o compression=zstd-5
-o recordsize=16K        # Small compiled shader files
-o checksum=fletcher2    # Fast integrity check
```

**Snapshot behavior:**  Excluded. Shader caches rebuild automatically.

#### Save Games (`~/Games`, `~/Wine-prefix`)
```bash
-o compression=zstd-4
-o recordsize=32K-1M     # Varies by dataset
-o copies=1
```

**Snapshot behavior:** Protect your 200-hour save files!

---

## Snapshot Strategy

### What Gets Snapshotted

| Dataset | Snapshots | Reason |
|---------|-----------|--------|
| `ROOT/default` | Yes | System rollback |
| `varlib` | Yes | Application state, Dockers, ollama |
| `opt` | Yes | Third-party software |
| `home` | Yes | User data |
| `home/config` | Yes | Settings recovery |
| `home/documents` | Yes | Important files and Game's save files |
| `home/wine-prefix` | Yes | Wine configurations |
| `home/local/steam/proton` | Yes | Compatibility tools |

### What's Excluded

| Dataset | Snapshots | Reason |
|---------|-----------|--------|
| `varcache` | No | Regeneratable |
| `varlog` | No | Historical only |
| `tmp` | No | Ephemeral |
| `vartmp` | No | Ephemeral |
| `home/games` | no | Re-downloadable, huge |
| `home/cache` | No | Regeneratable |
| `home/downloads` | No | Transient files |
| `home/local/steam/game` | No | Re-downloadable, huge |
| `home/local/steam/shadercache` | No | Regeneratable |

---

## Rollback Scenarios

### Scenario 1: Broken System Update
```bash
# Rollback only the system, keep /var/lib databases intact
zfs rollback nordix/ROOT/default@before-update
```
Your MariaDB data, Docker volumes, and application state remain untouched.

### Scenario 2: Broken Desktop Config
```bash
# Rollback only ~/.config
zfs rollback nordix/home/config@yesterday
```
Your documents, downloads, and game saves are unaffected.

### Scenario 3: Corrupted Game Save
```bash
# Rollback only your Games folder
zfs rollback nordix/home/dokuments@before-boss-fight
```
System, configs, and installed games remain current.

### Scenario 4: Bad Proton Version
```bash
# Rollback Proton compatibility tools
zfs rollback nordix/home/local/steam/proton@working-version
```
Installed games and save data unaffected.

---

## ZFS Hardening Strategy

Nordix implements multiple layers of data protection:

### Layer 1: Metadata Redundancy (All Datasets)
```
redundant_metadata=most
```
Every dataset inherits this from the pool. All metadata blocks (directory entries, file attributes, indirect blocks) are stored with redundant copies. Metadata corruption is automatically repaired.

### Layer 2: Data Redundancy (Critical Datasets)
```
copies=2 on nordix/ROOT/default
copies=2 on nordix/opt
```
Critical system data is stored twice. ZFS can recover from single-block corruption and gains read performance by choosing the faster copy.

### Layer 3: Checksums (All Datasets)
```
checksum=fletcher4
```
Every block is checksummed. ZFS detects silent corruption that would go unnoticed on ext4/NTFS/BTRFS. When combined with `copies=2` or `redundant_metadata=most`, detected corruption is automatically repaired.

### Layer 4: Security Properties
```
exec=off      # No code execution from data directories
setuid=off    # No setuid binaries
devices=off   # No device files
```
Applied to user data datasets (Pictures, Videos, Music, Downloads, cache directories) to prevent malicious files from executing.

### Protection Matrix

| Dataset | Metadata Redundancy | Data Copies | Checksum | Security Flags |
|---------|---------------------|-------------|----------|----------------|
| ROOT/default | yes | 2 | fletcher4 | — |
| opt | yes | 2 | fletcher4 | — |
| varlib | yes | 1 | fletcher4 | setuid=off |
| home | yes | 1 | fletcher4 | setuid=off, devices=off |
| home/config | yes | 1 | fletcher4 | exec=off, setuid=off, devices=off |
| home/pictures | yes | 1 | fletcher4 | exec=off, setuid=off, devices=off |
| tmp | yes | 1 | off | exec=off, setuid=off, devices=off |

---

## Useful Commands

```bash
# List all datasets with compression ratios
zfs list -o name,used,compressratio,mountpoint

# Show snapshot space usage
zfs list -t snapshot -o name,used,refer

# Create manual snapshot before risky operation
zfs snapshot nordix/ROOT/default@before-experiment

# Rollback to snapshot
zfs rollback nordix/ROOT/default@before-experiment

# Compare snapshot to current state
zfs diff nordix/home/config@yesterday

# Destroy old snapshots
zfs destroy nordix/ROOT/default@old-snapshot
```

---

## Performance Tuning Notes

### Record Size Selection

| Record Size | Best For |
|-------------|----------|
| 16K | Databases, small random I/O |
| 32K | Config files, mixed workloads |
| 128K | General system files |
| 1M | Game assets, large binaries |
| 4M | Video files, streaming media |

### Compression Selection

| Algorithm | Speed | Ratio | Best For |
|-----------|-------|-------|----------|
| `lz4` | Fastest | Low | Temp, media, already-compressed |
| `zstd-1` | Fast | Medium | General cache, home |
| `zstd-5` | Balanced | Good | System, documents |
| `zstd-10` | Slow | Excellent | Logs, archival |

### Cache Strategy

| Setting | Meaning |
|---------|---------|
| `primarycache=all` | Cache data + metadata in RAM (ARC) |
| `primarycache=metadata` | Cache only metadata (save RAM for other uses) |
| `secondarycache=all` | Use L2ARC (SSD cache) for data |
| `secondarycache=none` | Don't use L2ARC |

---

## License
 * SPDX-License-Identifier: GPL-3.0-or-later                           
 * [**Nordix - license**](https://www.gnu.org/licenses/gpl-3.0.html) 
 * Copyright (c) 2025- The Nordix Authors                                                              
 * Part of Nordix - https://github.com/jimmykallhagen/Nordix                        


---
