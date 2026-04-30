**Part of:** [Nordix](https://github.com/jimmykallhagen/Nordix)  
**Author:** Jimmy Källhagen  
**License:** GPL-3.0-or-later


![nordix-cool-super-HHD](https://github.com/jimmykallhagen/nordix-zfs/blob/main/nordix-hhd.png)


# ZPOOL Stipe - HHD - SSD Special Vdev 

---
# **This is a guide to creating an optimal and fast additional storage volume for home use**
---

## **REDUNDACY**
>If you have very important data you may need to consider some form of redundancy, in my opinion you want to run HHD in stripe to be able to have a nice home system, but there are solutions to achieve this with other raid configurations in combination with l2arc, special vdev and slog in combination with the options sync=always on the dataset, gives you a zpool with rudundas and very smooth use and masks any limited performance, but it requires you to build your entire computer for this very purpose. If you instead want to create a simple but effective storage pool with simple means, I have an alternative solution to the redundancy problem. This involves partitioning your HHD into two parts, one that you run stripe on and a smaller one that you run mirror or raidz1. You can imagine that most of the media, such as Steam games libraries etc. can be downloaded again.
so you can now run it on the stripe part and then actually be able to use your HHD effectively for gaming, then have a second partition on your HHD which is then preferably run with zfs raidz1, you can then put critical data on it and then be safe

 - [**Stripe and Raidz1 for Redundancy Guide**](stripe-raidz1-setup.md)

---

**Reach your system's full storage pool potential with ZFS**

 - This is my own setup for a HHD zpool, 3 HGST 8TB, used with between 20-25 thousand hours of operation,
no bad sections, so they are perfect for acting as a fast, nice home lab zpool

> Remember that the recommendations for ZFS are geared towards server and enterprise use,
for a regular home computer you will probably never have problems with healthy drives in the stripe.

---

> ⚠️ </br>
**_Always make sure your HHDs have good ventilation and that their mounting has vibration damping, if any of this is lacking in your setup you can drastically minimize the lifespan of your HHDs._** </br>
> 🚫 </br>
**_Never expose your HHD to shocks and make sure to never drop one while mounting, this is guaranteed to destroy it instantly._**
---
According to my tests with compression, you get varied results on media and games, 
but one factor that is the same is that it does not give a better compression ratio with a higher degree of compression, 
so for media and games I like to set fast compression, zstd-fast goes up to 1000 if you want to test it.
I recommend not using primarycache=all for a storage pool, it will fill up the ARC with files that are not important.
Instead I recommend using l2arc for hhd pools.
If you already have your system on an nvme you can create a zvol and use it as a cache for HHD (l2arc).
>_will add a guide to create a temporary l2arc for your storage pool later_
---
## **Nordix - Zpool Storage Guide**
 _**1️⃣**_
 - **Locate your drives**
```Fish
lslbk
```
For me it looks like this, it might look different for you:
```Fish
┏━[❄️]━[core]━[:11:07:34
┣━[Nordix]━[❄️]━[~
┗❯lsblk
NAME        MAJ:MIN RM   SIZE RO TYPE MOUNTPOINTS
sda           8:0    0   7.3T  0 disk 
├─sda1        8:1    0   7.3T  0 part 
└─sda9        8:9    0    64M  0 part 
sdb           8:16   0 111.8G  0 disk 
├─sdb1        8:17   0 111.8G  0 part 
└─sdb9        8:25   0     8M  0 part 
sdc           8:32   0   7.3T  0 disk 
├─sdc1        8:33   0   7.3T  0 part 
└─sdc9        8:41   0    64M  0 part 
sdd           8:48   0 111.8G  0 disk 
├─sdd1        8:49   0 111.8G  0 part 
└─sdd9        8:57   0     8M  0 part 
sde           8:64   0   7.3T  0 disk 
├─sde1        8:65   0   7.3T  0 part 
└─sde9        8:73   0    64M  0 part 
sdf           8:80   1  28.9G  0 disk 
└─sdf1        8:81   1  1000M  0 part 
zd0         230:0    0   220G  0 disk 
├─zd0p1     230:1    0   100M  0 part 
├─zd0p2     230:2    0    16M  0 part 
└─zd0p3     230:3    0 219.9G  0 part 
nvme0n1     259:0    0 931.5G  0 disk 
├─nvme0n1p1 259:2    0 931.5G  0 part 
└─nvme0n1p9 259:3    0     8M  0 part 
nvme1n1     259:1    0 931.5G  0 disk 
├─nvme1n1p1 259:5    0 931.5G  0 part 
└─nvme1n1p9 259:6    0     8M  0 part 
nvme3n1     259:4    0 931.5G  0 disk 
├─nvme3n1p1 259:7    0 931.5G  0 part 
└─nvme3n1p9 259:8    0     8M  0 part 
nvme2n1     259:9    0 931.5G  0 disk 
├─nvme2n1p1 259:10   0 931.5G  0 part 
└─nvme2n1p9 259:11   0     8M  0 part
```
> _Be sure to locate your devices correctly here, otherwise it won't be fun._

The drives I will be using are
 - 3 8TB HGST HHD for storage
 - 2 Old SATA SSDs, one Intel 120GB and one Kingston 120GB as Special Vdev's

The 8TB HGST HHD is:
```Fish
/dev/sda
/dev/sdc
/dev/sde
```
The SATA SDD is:
```Fish
/dev/sdb
/dev/sdd
```
---
**_2️⃣_**
 - **Erase the drives**

```Fish
sudo wipefs -a --force /dev/sda
sudo wipefs -a --force /dev/sdc
sudo wipefs -a --force /dev/sde
sudo wipefs -a --force /dev/sdb
sudo wipefs -a --force /dev/sdd
```
> _With the --force flag, you may need to run the command twice to be sure you have forced your deletion_
---
**_3️⃣_**
 - Disk By-Id
> According to the current ZFS documentation, they recommend not creating zpools with device /dev/sdX or /dev/nvmeX, but instead it is recommended to use disk by-id. It is possible to use device name but it can cause problems. Make it a practice to always create zpools with disk By-Id.
We use this command to display the disk by-id of our devices:
```Fish
ls -l --time-style=+ /dev/disk/by-id/ | grep sda
```

> If you have cleaned up your devices correctly as I showed before, only two options will appear when we run this command.
> _I couldn't come up with a better way to sort out the different ids than what this command does, it's not perfect but good enough_ 
**So for me it looks like this**

 - HHD
```Fish
ls -l --time-style=+ /dev/disk/by-id/ | grep sda
lrwxrwxrwx 1 root root  9  ata-HUH728080ALN600_2EHXRH1X -> ../../sda
lrwxrwxrwx 1 root root  9  wwn-0x5000cca23bdb264d -> ../../sda
```
```Fish
ls -l --time-style=+ /dev/disk/by-id/ | grep sdc
lrwxrwxrwx 1 root root  9  ata-HUH728080ALN600_VJG1U0SX -> ../../sdc
lrwxrwxrwx 1 root root  9  wwn-0x5000cca261c0d24f -> ../../sdc
```
```Fish
ls -l --time-style=+ /dev/disk/by-id/ | grep sde
lrwxrwxrwx 1 root root  9  ata-HUH728080ALN600_VJG1PJBX -> ../../sde
lrwxrwxrwx 1 root root  9  wwn-0x5000cca261c0c52f -> ../../sde
```
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
For SATA Devices it is "ata" that we will use. My list will then look like this

  - **HHD**
```Fish
 /dev/disk/by-id/ata-HUH728080ALN600_2EHXRH1X
 /dev/disk/by-id/ata-HUH728080ALN600_VJG1U0SX
 /dev/disk/by-id/ata-HUH728080ALN600_VJG1PJBX
```
  - **SSD**
```Fish
 /dev/disk/by-id/ata-INTEL_SSDSC2CW120A3_CVCV430601BD120BGN
 /dev/disk/by-id/ata-KINGSTON_SA400S37120G_50026B767B0067D9
```
> _If you are not using SATA devices but perhaps NVME or USB, replace "ata" with nvme or usb._
> _If you feel unsure about always using the terminal, it is also fine to use your file manager and then locate your drive in the directory /dev/disk/by-id/ and then copy the entire search path to it to use in the zpool creation._
---
**_4️⃣_**
 - **Zpool create tank**
This will create a fast Stripe HHD pool, make sure your HHD is in good condition and that it is okay. 
We set a number of options here that are not zpool options with -O, these are dataset options and we set them now so that all the datasets we create later inherit these options, this means that you do not need to set these options anymore than when creating this zpool. If you want other options or individual options on your datasets, it is perfectly possible to only set them on the current dataset and these options will then be overriden

**⚠️**
>Remember to always set ashift correctly, modern NVME SSD, SATA SDD and HHD usually always have 4k (4096) sectors, your devices will lie to you and the system and often show 512 sectors, this is to be compatible with windows, windows still runs today with the same kernel that was released in 1993, therefore, as a hardware manufacturer, you must comply with these ancient specifications. 
you cannot trust what the system says that your device has for sectors.
> To be completely sure what your devices have for sectors, go to your manufacturer's website and check the specifications, however, it is very safe to believe that you have 4k (4096) sectors and there we set ashift=12, some of the very latest and sharpest NVME can have 8K (8192) sectors and then you set ashift=13
- This will create and format a zpool, here we only include the devices that will act as storage units, as a rule of thumb use units of the same type, size and specifications. You can mix whatever you want but it will not be to your advantage to do so
- I set the name tank then I use the disk by-id I previously sorted out for my HHD and set them in a row with a space between each after tank, if you want a different name than tank then just change it, tank is however a standard name for a storage pool in ZFS

 - Create zpool:
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
tank /dev/disk/by-id/ata-HUH728080ALN600_2EHXRH1X /dev/disk/by-id/ata-HUH728080ALN600_VJG1PJBX /dev/disk/by-id/ata-HUH728080ALN600_VJG1U0SX
```
> _I have set the option autoexpand=on
even though this is a stripe pool, this is because I will run my special vdev's in mirror and if I later want to upgrade to a larger SSD, it will now automatically accept a larger SSD_
---
**_5️⃣_**
**Special Vdev** - _a must_
- This will add two SATA sdd disks in mirror for special vdevs, I prefer to just store metadata on them.
- This is a large HHD pool for Virtual Machines, media and gaming, so setting the options to store small files on special vdevs is not really necessary.
> Using SSD/NVME to store metadata on a HHD pool is something you should consider doing
as it gives a huge gain in performance on latency and your large HHD pool now becomes a much nicer pool for both games and Virtual Machines.
I'm using older SATA SSDs (an old Intel and an old Kingstone) here that I don't completely trust, so I put them in a mirror,
then one can break and you have the chance to replace it, mirroring also gives increased read speed,
like stripe but only for reads, not for writes, which is not relevant for special vdev anyway.
> ⚠️ </br>
> _Remember that if you use a special vdev for metadata, all data on the entire zpool will be lost if you delete or remove this special vdev. </br>
> Always set ashift for your special vdefv to match the zpool they are to be connected to in our zpool configuration
 
 - Add Special vdev for metadata only
```Fish
sudo zpool add -f \
-o ashift=12 \
tank special mirror /dev/disk/by-id/ata-INTEL_SSDSC2CW120A3_CVCV430601BD120BGN /dev/disk/by-id/ata-KINGSTON_SA400S37120G_50026B767B0067D9
```
---
**_6️⃣_**
 - **Create a Dataset** 
Now we can create a dataset on this awesome tank and since we already set up the dataset options on this tank, they inherit these when we create new datasets, only if we want changed options do we need to set them, this time we run as the pool is prepared
 - Import your zpool once with force, so that zfs will include it in its cache
```Fish
sudo zpool export tank
sudo zpool import -f tank
```
I want to create a dataset for my media library, I name this library. I also want this to be automatically mounted in my home so I set canmount=on and set the mountpoint in my home under Library

- Create dataset "library"
```Fish
sudo zfs create \
-o mountpoint=/home/core/Library \
-o canmount=on \
tank/library
```
- Make your user the owner:
```Fish
sudo chown core:core -R /home/core/Library/
```
---
**_7️⃣_**
- **The Test**
  
Now I got a directory mounted in my home called Library, can we test this with nx-mv and see if we managed to get satisfactory results

This runs with only primarycache=metadata

 - Screeshot - Simple Test with nx-mv 
![1](https://github.com/jimmykallhagen/nordix-zfs/blob/main/Screenshot-Wed%20Apr%2029%2009%3A45%3A38%20PM%20UTC%202026.png)
![2](Screensho.png)
![3](https://github.com/jimmykallhagen/nordix-zfs/blob/main/Screenshot-Wed%20Apr%2029%2009%3A46%3A20%20PM%20UTC%202026.png)
I myself am very happy with the result and have now got a storage pool of 24TB that can be used for everything from Virtual machines, backups, game libraries, media libraries, a zpool like this is capable to run full OS on if you want to have more BE. ZFS really offers world class storage solutions
> If you want to take this further you can create an l2arc, slog, larger special vdev and then put small files on the ssd also to minimize the bottlenecks on your HHD, you can also set dataset options:
copies=2 
this means that you create two of each file, this is to increase the redundancy when checksum but also to increase the read speed considerably on the HHD, you can by setting copies=2 increase the read speed by up to 100%, you will however get worse write speed and you will get less storage capacity, but if you run virtual machines and your zpool is running a little too slowly, copies=2 can be a solution that makes your virtual machine run smoothly
---

# ⚠️ _-_ **_YOU NEED THIS:_**
> **ZFS is its own system in your system, and since ZFS is the best system we have for managing files and storage devices, it should also be allowed to use its own scheduler, so it is important that you apply this udev rule in your system.**

  - [**UDEV-RULE**](https://github.com/jimmykallhagen/nordix-zfs/tree/main/zfs-specific-udev-rules)
