# Changelog

All notable changes to Nordix are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- **LICENSE: Relicensed from PolyForm Noncommercial 1.0.0 to GPL-3.0-or-later.**
  As the sole copyright holder, Jimmy Källhagen has relicensed all Nordix
  source code, configuration, and documentation under the GNU General Public
  License, version 3 or later. This change applies to all commits on or after
  2026-04-21. Versions released before that date remain available under the
  previous PolyForm Noncommercial license and are not retroactively relicensed.
  Trademarks ("Nordix", "Yggdrasil") are retained separately from the code
  license — see README.md for details.

- **ZFS: Updated kernel module parameters for OpenZFS 2.2.0+.**
  The `modprobe.d/zfs-*gb.conf` profiles (8/16/32/64/128 GB) have been updated
  to remove parameters deprecated in OpenZFS 2.2.0 (commit `a8d83e2a`,
  "More adaptive ARC eviction") and later. These parameters are silently
  ignored by the ZFS module and produce "unknown parameter ignored" warnings
  in `dmesg` on recent kernels.

### Removed

- ZFS parameter `zfs_arc_meta_limit_percent` (replaced by `zfs_arc_meta_balance`)
- ZFS parameter `zfs_arc_meta_prune` (replaced by adaptive eviction)
- ZFS parameter `zfs_arc_meta_adjust_behavior` (replaced by adaptive eviction)
- ZFS parameter `zfs_resilver_delay` (replaced by NIA scheduler)
- ZFS parameter `zfs_scrub_delay` (replaced by NIA scheduler)
- ZFS parameter `zfs_scan_idle` (replaced by NIA scheduler)

### Added

- ZFS parameter `zfs_arc_meta_balance=500` in all RAM profiles — adaptive
  metadata/data eviction balance knob that replaces the three removed
  `zfs_arc_meta_*` parameters. Default value preserves reasonable behavior;
  raise to 1500–2500 for metadata-heavy workloads (compile servers, git).
- Documentation for the NIA (Non-Interactive I/O) scheduler parameters
  (`zfs_vdev_nia_delay`, `zfs_vdev_nia_credit`) in the scrub/resilver section.
- `REQUIRES: OpenZFS 2.2.0 or newer` notice in each `zfs-*gb.conf` header.
- Standard GPLv3+ notice block in each configuration file header.
- `LICENSE` file containing the full GPL-3.0 text.

### Migration notes

Users upgrading from the pre-relicense version of Nordix:

1. The configuration files are drop-in compatible — same parameter semantics,
   just fewer parameters. No `zpool` or dataset changes are required.
2. Rebuild your initramfs after updating `/etc/modprobe.d/zfs.conf`:
   ```
   sudo mkinitcpio -P
   ```
3. Reboot (or `sudo modprobe -r zfs && sudo modprobe zfs` if ZFS is not
   mounted, which is rarely the case on ZFS-on-root systems).
4. Verify the new parameters are active:
   ```
   cat /sys/module/zfs/parameters/zfs_arc_meta_balance
   ```
5. Confirm the deprecated warnings are gone:
   ```
   dmesg | grep "unknown parameter"
   ```
