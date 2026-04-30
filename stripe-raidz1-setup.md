**Part of:** [Nordix](https://github.com/jimmykallhagen/Nordix)  
**Author:** Jimmy Källhagen  
**License:** GPL-3.0-or-later

# ZFS-Stripe combined with ZFS-Raidz1

> In this Guide we will create a combined Stripe/Raidz1 zpool with Special vdev on the Stripe pool
>
> This is so that we can run a fast stripe zpool but at the same time have part of the storage space with stripe data to store sensitive data on.

---
If you haven't read my previous ZFS storage guide, do that first, in case there is anything unclear in this guide.
 - [**Nordix ZFS Storage Guide**](/nordix-tank.md)
---

## To break away from the terminal a bit, I will now start the guide by showing how I partitioned my HHD with the partition manager program.

If you dont have partitionmanager,you need to install it:
```Fish
sudo pacman -Sy partitionmanager
```

Once we have started the partition manager, we will find our drive. What we are going to do now you must do the same on all the HHDs you are going to include in your zpool. Be sure to choose the right device here otherwise it will be wrong.

## 1. I start by creating a partition scheme
 - We will create a GPT Schema

![1](https://github.com/jimmykallhagen/nordix-zfs/blob/main/screenshots/1.png)

## 2. After we have created a partition scheme, we will now create the first partition.
![2](https://github.com/jimmykallhagen/nordix-zfs/blob/main/screenshots/2.png)

 - I choose to put the stripe at the farthest end and also give it the most space.
 - We format all partitions as unformatted
![3](https://github.com/jimmykallhagen/nordix-zfs/blob/main/screenshots/3.png)

## 3. Now we can see that the first partition is made, the brown part that remains is unformatted, right click and select new partition
 - Format everything that remains as an unformatted partition
![4](https://github.com/jimmykallhagen/nordix-zfs/blob/main/screenshots/4.png)

## 5. This is what we want to achieve, two unformatted partitions made with GPT partition scheme.
 - You repeat this on all HHD that you will use in your zpool
 - Adjust the partitions for what you want to allocate for size of storage volume to the different raid configurations to the different raid configurations
![5](https://github.com/jimmykallhagen/nordix-zfs/blob/main/screenshots/5.png)

---

**Now we will use the same approach as we did for our ZFS Stripe pool, the only difference is that we will also create a ZFS Raidz1 pool as well.**
 - I will therefore not be as clear with all the steps in this Guide 
 - [**Nordix ZFS Storage Guide**](https://github.com/jimmykallhagen/nordix-zfs/blob/main/nordix-tank.md)

---

To create a ZFS Raidz1 pool, you must have at least three drives.
If you only have two, you can create your redundancy pool with mirrors instead.

---

**Find THe disk by-id**
```Fish
ls -l --time-style=+ /dev/disk/by-id/ | grep sdb
```

**This is how my devices looks like now:**
```Fish
ls -l --time-style=+ /dev/disk/by-id/ | grep sda
lrwxrwxrwx 1 root root  9  ata-HUH728080ALN600_2EHXRH1X -> ../../sda
lrwxrwxrwx 1 root root 10  ata-HUH728080ALN600_2EHXRH1X-part1 -> ../../sda1
lrwxrwxrwx 1 root root 10  ata-HUH728080ALN600_2EHXRH1X-part2 -> ../../sda2
lrwxrwxrwx 1 root root  9  wwn-0x5000cca23bdb264d -> ../../sda
lrwxrwxrwx 1 root root 10  wwn-0x5000cca23bdb264d-part1 -> ../../sda1
lrwxrwxrwx 1 root root 10  wwn-0x5000cca23bdb264d-part2 -> ../../sda2
```
```Fish
ls -l --time-style=+ /dev/disk/by-id/ | grep sdc
lrwxrwxrwx 1 root root  9  ata-HUH728080ALN600_VJG1U0SX -> ../../sdc
lrwxrwxrwx 1 root root 10  ata-HUH728080ALN600_VJG1U0SX-part1 -> ../../sdc1
lrwxrwxrwx 1 root root 10  ata-HUH728080ALN600_VJG1U0SX-part2 -> ../../sdc2
lrwxrwxrwx 1 root root  9  wwn-0x5000cca261c0d24f -> ../../sdc
lrwxrwxrwx 1 root root 10  wwn-0x5000cca261c0d24f-part1 -> ../../sdc1
lrwxrwxrwx 1 root root 10  wwn-0x5000cca261c0d24f-part2 -> ../../sdc2
```
```Fish
ls -l --time-style=+ /dev/disk/by-id/ | grep sde
lrwxrwxrwx 1 root root  9  ata-HUH728080ALN600_VJG1PJBX -> ../../sde
lrwxrwxrwx 1 root root 10  ata-HUH728080ALN600_VJG1PJBX-part1 -> ../../sde1
lrwxrwxrwx 1 root root 10  ata-HUH728080ALN600_VJG1PJBX-part2 -> ../../sde2
lrwxrwxrwx 1 root root  9  wwn-0x5000cca261c0c52f -> ../../sde
lrwxrwxrwx 1 root root 10  wwn-0x5000cca261c0c52f-part1 -> ../../sde1
lrwxrwxrwx 1 root root 10  wwn-0x5000cca261c0c52f-part2 -> ../../sde2
````

***This is what we should choose if we are creating the storage volume from SATA devices**
So since we have now divided each HHD into two partitions, we now have to take and group them correctly

 - HHD
For my stripe pool I will now use:
```Fish
/dev/disk/by-id/ata-HUH728080ALN600_2EHXRH1X-part1
/dev/disk/by-id/ata-HUH728080ALN600_VJG1U0SX-part1
/dev/disk/by-id/ata-HUH728080ALN600_VJG1PJBX-part1
```

For my Raidz1 pool I will now choose:
```Fish
/dev/disk/by-id/ata-HUH728080ALN600_2EHXRH1X-part2 
/dev/disk/by-id/ata-HUH728080ALN600_VJG1U0SX-part2
/dev/disk/by-id/ata-HUH728080ALN600_VJG1PJBX-part2 
```

---

**I now need to find my SSDs that will run as Special vdev in mirror of each other to my Stripe zpool**
 - SSD
```Fish
ls -l --time-style=+ /dev/disk/by-id/ | grep sdb
lrwxrwxrwx 1 root root  9  ata-INTEL_SSDSC2CW120A3_CVCV430601BD120BGN -> ../../sdb
lrwxrwxrwx 1 root root  9  wwn-0x55cd2e404bd41d9a -> ../../sdb
```
```Fish
ls -l --time-style=+ /dev/disk/by-id/ | grep sdd
lrwxrwxrwx 1 root root  9  ata-KINGSTON_SA400S37120G_50026B767B0067D9 -> ../../sdd
lrwxrwxrwx 1 root root  9  wwn-0x50026b767b0067d9 -> ../../sdd
```

**And the id i will use for my SSD's is:**
```Fish
 /dev/disk/by-id/ata-INTEL_SSDSC2CW120A3_CVCV430601BD120BGN
 /dev/disk/by-id/ata-KINGSTON_SA400S37120G_50026B767B0067D9
```
---

## Create the ZFS Stripe zpool

 - ZFS - Stripe
```Fish
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
tank /dev/disk/by-id/ata-HUH728080ALN600_2EHXRH1X-part1 /dev/disk/by-id/ata-HUH728080ALN600_VJG1U0SX-part1 /dev/disk/by-id/ata-HUH728080ALN600_VJG1PJBX-part1
```


## Create the ZFS Raidz1 zpool
 - I'm naming this zpool with rudundance tank-z1 because we're using ZFS Raidz1
 - If you only have two drives here you can use mirror instead
 
 - ZFS Raidz1
```Fish
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
tank-z1 raidz1 /dev/disk/by-id/ata-HUH728080ALN600_2EHXRH1X-part2 /dev/disk/by-id/ata-HUH728080ALN600_VJG1U0SX-part2 /dev/disk/by-id/ata-HUH728080ALN600_VJG1PJBX-part2 
```

----

**Now we have created two zpools that share drives.**

---

**You can now see when I run zpool status that we have succeeded in the mission.**

```Fish
┏━[❄️]━[core]━[:20:00:17
┣━[Nordix]━[❄️]━[~
┗❯zpool status

  pool: nordix
 state: ONLINE
 config:

	NAME                                            STATE     READ WRITE CKSUM
	nordix                                         ONLINE       0     0     0
	 nvme-WDS100T1X0E-00AFY0_20476B802969          ONLINE       0     0     0
	 nvme-KINGSTON_SFYRS1000G_50026B76869480A7     ONLINE       0     0     0
	 nvme-Samsung_SSD_980_PRO_1TB_S5GXNF0R605071H  ONLINE       0     0     0
	 nvme-KINGSTON_SFYRS1000G_50026B7686948127     ONLINE       0     0     0

errors: No known data errors

  pool: tank
 state: ONLINE
 config:

	NAME                                  STATE     READ WRITE CKSUM
	tank                                 ONLINE       0     0     0
	 ata-HUH728080ALN600_2EHXRH1X-part1  ONLINE       0     0     0
	 ata-HUH728080ALN600_VJG1U0SX-part1  ONLINE       0     0     0
	 ata-HUH728080ALN600_VJG1PJBX-part1  ONLINE       0     0     0

errors: No known data errors

  pool: tank-z1
 state: ONLINE
 config:

	NAME                                    STATE     READ WRITE CKSUM
	tank-z1                                ONLINE       0     0     0
	 raidz1-0                              ONLINE       0     0     0
	   ata-HUH728080ALN600_2EHXRH1X-part2  ONLINE       0     0     0
	   ata-HUH728080ALN600_VJG1U0SX-part2  ONLINE       0     0     0
	   ata-HUH728080ALN600_VJG1PJBX-part2  ONLINE       0     0     0

errors: No known data errors
```

---

Now we will also fix so that my two SATA SSDs are connected as special vdev to the tank (stripe pool)

 - Add Special vdev for metadata only
```Fish
sudo zpool add -f \
-o ashift=12 \
tank special mirror /dev/disk/by-id/ata-INTEL_SSDSC2CW120A3_CVCV430601BD120BGN /dev/disk/by-id/ata-KINGSTON_SA400S37120G_50026B767B0067D9
```

---

**Done, now we have a fast stripe zpool for VM, Gaming and everything that is not critical data, and at the same time we have created some of these HHD as a safe storage volume which then became 2TB**





