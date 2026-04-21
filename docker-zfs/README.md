# Docker on ZFS

## These are a few necessary steps to run Docker on ZFS

---

# 1.

- Tell Docker to use ZFS drivers.
- And of course, some Nordix memory optimizations


```
{
  storage-driver: zfs,
  storage-opts: [
    zfs.fsname=pool/docker
  ],
  default-ulimits: {
    nofile: {
      Name: nofile,
      Hard: 1048576,
      Soft: 1048576
    }
  }
}

```

You can find the entire file here:
- [**daemon.json**](https://github.com/jimmykallhagen/nordix-zfs/blob/main/docker-zfs/docker/daemon.json)

Put the file in:

- /etc/docker/daemon.json

---

# 2.


**This needs to be changed to allow ZFS to accept overlays<br>
which is necessary when running Docker with ZFS drivers.**

**From:**
- DO_OVERLAY_MOUNTS='no'

**To:**
- DO_OVERLAY_MOUNTS='yes'

---

**A clip from the /etc/default/zfs file showing the affected part:**
```
# Should we allow overlay mounts?
# This is standard in Linux, but not ZFS which comes from Solaris where this
# is not allowed).
DO_OVERLAY_MOUNTS='yes'
```

You can find the entire file here:
- [**zfs**](https://github.com/jimmykallhagen/nordix-zfs/blob/main/docker-zfs/default/zfs)

Put the file in:
- /etc/default/zfs

---

# 3.

```
sudo reboot
```
