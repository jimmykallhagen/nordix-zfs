# ZPOOL - HHD

This is my gna setup for a HHD zpool, 3 HGST 8TB, used with between 20-25 thousand hours of operation,
no bad sections, so they are perfect for acting as a fast, nice home lab zpool

Remember that the recommendations for ZFS are geared towards server and enterprise use,
for a regular home computer you will probably never have problems with healthy drives in the stripe.

According to my tests with compression, you get varied results on media and games, 
but one factor that is the same is that it does not give a better compression ratio with a higher degree of compression, 
so for media and games I like to set fast compression, zstd-fast goes up to 1000 if you want to test it.

I recommend not using primarycache=all for a storage pool, it will fill up the ARC with files that are not important.
Instead I recommend using l2arc for hhd pools.

If you already have your system on an nvme you can create a zvol and use it as a cache for HHD (l2arc).

locate your drives and clean them first, be sure to get the right one otherwise it will be wrong.

locate:
```bash
lslbk
```
Clean:
```bash
sudo wipefs -a --force /dev/sdX
```

## Zpool create tank
This will create a fast Stripe hhd pool, make sure your hhd is in good condition and that it is okay. 
It will prepare options for the dataset you create later, so you don't have to specify options for each dataset if you don't want to.
it is recomend to use disk by-id when you create a zpool.

An easy way to check this is to go to directory /dev/disk/by-id/
and look for your current drive, copy it and use the full path in the zpool creation

Create zpool:
```bash
sudo zpool create -f -o ashift=12 \
-o autotrim=on \
-o feature@large_dnode=enabled \
-O redundant_metadata=most \
-O dnodesize=auto \
-O mountpoint=none \
-O compression=zstd-fast-500 \
-O recordsize=1M \
-O atime=off \
-O xattr=sa \
-O sync=standard \
-O checksum=fletcher4 \
-O acltype=off \
-O logbias=throughput \
-O primarycache=metadata \
-O secondarycache=all \
-o autoexpand=on \
tank /dev/disk/by-id/ata-HUH728080ALN600_2EHXRH1X /dev/disk/by-id/ata-HUH728080ALN600_VJG1PJBX /dev/disk/by-id/ata-HUH728080ALN600_VJG1U0SX
```

## **Special Vdev** - _a must_

This will add two SATA sdd disks in mirror for special vdevs, I prefer to just store metadata on them.
This is a large HHD pool for Virtual Machines, media and gaming, so setting the options to store small files on special vdevs is not really necessary.

Using SSD/NVME to store metadata on a HHD pool is something you should consider doing
as it gives a huge gain in latency and your large HHD pool now becomes a much nicer pool for both games and Virtual Machines.

I'm using older SATA SSDs (an old Intel and an old Kingstone) here that I don't completely trust, so I put them in a mirror,
then one can break and you have the chance to replace it, mirroring also gives increased read speed,
like stripe but only for reads, not for writes, which is not relevant for special vdev anyway.

remember that if you use a special vdev for metadata, all data on the entire zpool will be lost if you delete this special vdev

Special Vdev:
```bash
sudo zpool add -f -o ashift=12 tank special mirror /dev/disk/by-id/ata-INTEL_SSDSC2CW120A3_CVCV430601BD120BGN /dev/disk/by-id/ata-KINGSTON_SA400S37120G_50026B767B0067D9
```

## Dataset 

now we can create a dataset on this awesome tank and since we already set up the dataset options on this tank, they inherit these when we create new datasets, only if we want changed options do we need to set them, this time we run as the pool is prepared

I want to create a dataset for my media library, I name this library. I also want this to be automatically mounted in my home so I set canmount=on and mountpoint in my home under Library

Create dataset "library"
```bash
sudo zfs create \
-o mountpoint=/home/core/Library \
-o canmount=on \
tank/library
```
Make your user the owner:
```bash
sudo chown core:core -R /home/core/Library
```

Now I got a directory mounted in my home called Library, can we test this with nx-mv and see if we managed to get satisfactory results

![1](https://github.com/jimmykallhagen/nordix-zfs/blob/main/Screenshot-Wed%20Apr%2029%2009%3A45%3A38%20PM%20UTC%202026.png)
![2](Screensho.png)
![3]("/Screenshot-Wed Apr 29 09:46:20 PM UTC 2026.png")
