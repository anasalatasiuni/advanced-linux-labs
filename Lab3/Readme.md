# Lab 3

## Objective

To implement a full embedded Linux boot process using QEMU with:

* U-Boot (bootloader)
* Linux kernel compiled for ARM
* Initramfs with custom `/init` script
* Root filesystem (`rootfs.ext4`)

### Expected Boot Flow:

**U-Boot → Kernel → Initramfs → switch\_root → Rootfs**

---

## Environment Setup

Installed required tools:

![alt text](image_2025-05-09_21-48-37.png)
---

## Building U-Boot

```bash
git clone https://github.com/u-boot/u-boot.git
cd u-boot
git checkout v2022.01

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make vexpress_ca9x4_defconfig
make 
```
![alt text](image_2025-05-09_21-55-02.png)
![alt text](image_2025-05-09_22-18-28.png)
![alt text](image_2025-05-09_22-19-43.png)
---

## Building Linux Kernel

```bash
git clone --depth=1 https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
cd linux

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make vexpress_defconfig
make zImage
make modules 
make dtbs 
```

![alt text](image_2025-05-09_22-31-07.png)
---

## Creating Initramfs with BusyBox

```bash
wget https://busybox.net/downloads/busybox-1.36.0.tar.bz2
tar -xvf busybox-1.36.0.tar.bz2
cd busybox-1.36.0

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make defconfig
make 
make install
```
![alt text](image_2025-05-09_22-46-47.png)
Wrote a  `/init` script inside `initramfs/`:

```sh
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

mkdir /newroot
mount -t ext4 /dev/mmcblk0p2 /newroot
exec switch_root /newroot /sbin/init
```

Packed initramfs:

```bash
cd initramfs
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
```
![alt text](image-3.png)
---

## Root Filesystem (`rootfs.ext4`)

```bash
mkdir -p rootfs/{bin,sbin,etc,proc,sys,usr/{bin,sbin},dev,tmp,home}
cp -a busybox-1.36.0/_install/* rootfs/
sudo mknod -m 666 rootfs/dev/console c 5 1
sudo mknod -m 666 rootfs/dev/null   c 1 3
```

Added:

**`/etc/inittab`**

```ini
::sysinit:/etc/init.d/rcS
::respawn:/bin/sh
```

**`/etc/init.d/rcS`**

```sh
#!/bin/sh
echo "[OK] rcS script started."
mount -t proc none /proc
mount -t sysfs none /sys
```
![alt text](image-4.png)
![alt text](image-5.png)
---

## Creating SD Image

```bash
dd if=/dev/zero of=sd.img bs=1M count=64
fdisk sd.img
# o, n p 1 +16M, n p 2 (default), t 1 c, w
```
![alt text](image-7.png)
```bash
LOOP=$(sudo losetup --find --show --partscan sd.img)
sudo mkfs.vfat -n BOOT ${LOOP}p1
sudo mkfs.ext4 -L ROOT ${LOOP}p2
```

### Filled Boot Partition:

```bash
mkimage -A arm -T ramdisk -C gzip -n "Initramfs" -d initramfs.cpio.gz uInitrd

sudo mount ${LOOP}p1 mnt
sudo cp linux/arch/arm/boot/zImage mnt/
sudo cp linux/arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb mnt/vexpress.dtb
sudo cp uInitrd mnt/
sudo umount mnt
```
![alt text](image-6.png)

### Filled Rootfs Partition:

```bash
sudo mount ${LOOP}p2 mnt
sudo cp -a rootfs/* mnt/
sudo umount mnt
sudo losetup -d "$LOOP"
```
![alt text](image_2025-05-09_23-20-55.png)
---

## Boot in QEMU with U-Boot

```bash
qemu-system-arm -M vexpress-a9 \
  -m 512M \
  -kernel u-boot/u-boot \
  -drive file=sd.img,format=raw,if=sd \
  -nographic
```
![alt text](image-1.png)
![alt text](image-2.png)
---

## Final Result: Booted Root Shell



![alt text](image.png)


# THANK YOU for this course! We learned alot of cool things.