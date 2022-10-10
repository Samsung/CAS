# CAS in virtual environment

`CAS` kernel tracing module can be used in virtual environment. Following guide will enable to use etrace in QEMU KVM.

Please be aware that most of following commands will require elevated permissions to run properly. 

# TLDR

Check `tools/virtual_environment/img_build.sh` script. Run it in empty directory to create the QEMU image in one shot. To know exactly what happens during image creation please follow the guide.

## Installing required packages on host system

```bash
sudo apt install libguestfs-tools qemu-utils qemu-system-x86
```

## Download image 
Following guide will use `Ubuntu Cloud Image` as root filesystem.

```bash
export IMG_BASE=base_system.img

curl -s -o "${IMG_BASE}" "https://cloud-images.ubuntu.com/minimal/releases/focal/release/ubuntu-20.04-minimal-cloudimg-amd64.img"
```

## Expand image partition size
To build `CAS` tracer and cas libraries there are some requirements that must be installed in guest system.
Unfortunately `Ubuntu Cloud Image` (and some other) have limited space on root partition.

Partition list can be checked using following command
```bash
$ virt-list-partitions --long --human --format=qcow2 ${IMG_BASE} 

/dev/sda1  ext4      2.1G
/dev/sda14 unknown   4.0M
/dev/sda15 vfat    106.0M
```

To expand root partition size (let's say to ~20GB) following commands must be processed
```bash
export IMG_RESIZED=system_resized.img

$ qemu-img create -f qcow2 ${IMG_RESIZED} 20G

$ virt-resize --format=qcow2 --expand /dev/sda1 ${IMG_BASE} ${IMG_RESIZED}
```

NOTE: Check if partition layout changed. Changed partition order will most likely cause error when booting image in QEMU.

Look for something like this in virt-resize log:
```bash
[  22.3] Expanding /dev/sda1 (now /dev/sda3) using the ‘resize2fs’ method
```

It's clear that root /dev/sda1 partition name changed to /dev/sda3. 

To fix possible booting problems we must reinstall grub - use CHANGED partition name (eg /dev/sda3) as mount source.
```bash
EXPANDED_PARTITION=/dev/sda3

virt-customize -a ${IMG_RESIZED} \
      --run-command "mkdir -p /mnt && mount ${EXPANDED_PARTITION} /mnt && mount --bind /dev /mnt/dev && mount --bind /proc /mnt/proc && mount --bind /sys /mnt/sys && chroot /mnt && grub-install /dev/sda"
```


After that, booting should be fixed. 

## Customize image

There is handy tool to customize images called `virt-customize`. 
Note, that each run of virt-customize creates qemu instance that inject some changes into image, then quits and commit changes.

From this point is highly recommend to work on copy of resized image, because in case of some error in virt-customize you won't have to resize image from begining.

```bash
export IMG_WORK=cas_tracer_env.img
cp ${IMG_RESIZED} ${IMG_WORK}
```

Note **`-smp ${PROC} -m ${MEM}`** parameters in following commands - it is good to speedup virt-customize by adding some cores and memory (by default it uses single core and ~700M RAM)

It is highly recommend to set some resonable values to $PROC and $MEM - for example 80% of system resources.
```bash
export MEM=$(free --mega | grep "Mem:" | awk '{printf "%.0f",($2 * 0.8)}')
export PROC=$(nproc | awk '{printf "%.0f",($1 * 0.8)}')
```

### Set root password example

```bash
virt-customize -a ${IMG_WORK} -smp ${PROC} -m ${MEM} \
   --root-password password:P@ss
```

### Install packages required by `CAS`
```bash
virt-customize -a ${IMG_WORK} -smp ${PROC} -m ${MEM} \
   --install "build-essential,linux-headers-generic,git,cmake,llvm,clang,clang-10,libclang-dev,python3-dev,gcc-9-plugin-dev,rsync,clang-11,flex,bison"
```

### Build CAS 
```bash
virt-customize -a ${IMG_WORK} -smp ${PROC} -m ${MEM} \
   --mkdir /opt/ \
   --run-command 'git clone https://github.com/Samsung/CAS.git /opt/cas/' \
   --run-command 'cd /opt/cas/tracer/ && make -C /lib/modules/$(ls /lib/modules/ | grep kvm)/build M=$PWD && make -C /lib/modules/$(ls /lib/modules/ | grep kvm)/build M=$PWD modules_install' \
   --run-command 'cd /opt/cas/bas/ && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ .. && make && make install' \
   --run-command 'cd /opt/cas/ && ./etrace_install.sh' \
   --firstboot-command "depmod -a"
```

### Enable network and ssh

```bash
virt-customize -a ${IMG_WORK} -smp ${PROC} -m ${MEM} \
   --run-command "sed -i 's/GRUB_CMDLINE_LINUX=/GRUB_CMDLINE_LINUX=\"net.ifnames=0 biosdevname=0\"/g' /etc/default/grub" \
   --run-command "update-grub" \
   --run-command "echo '
network:
  version: 2
  renderer: networkd
  ethernets:
    eth0:
      dhcp4: true
      optional: true
' > /etc/netplan/01-netcfg.yaml" \
   --firstboot-command "netplan apply" \
   --firstboot-command "dpkg-reconfigure openssh-server"
```
# Preparing source image

This step is not mandatory, User may use virtio mounts access host sources from guest. But in some cases (eg mmpaing while building AOSP) it may be required to use mounted image with sources.

Following example will describe how to create image with sources.

```bash 
virt-make-fs --format=qcow2 --size=+30G ~/source_dir/ src.img
```
This will build image with sources and additionally 30GB free space.

It is good to have some qcow2 overlay serving as mutable storage. In case of any errors while compiling it will not harm source src.img.
```bash
qemu-img create -f qcow2 -b src.img -F qcow2 work.qcow2
```

# Running KVM

After building root image and preparing some sources it's finall time to run VM. It is up to user how to run VM using raw command or tools like virsh.
Following command is example how user may run it in current terminal (useful for remote execution).

```bash
qemu-system-x86_64 --enable-kvm -nographic -cpu host -smp ${PROC} -m ${MEM} -drive file=${IMG_WORK} -drive file=work.qcow2 -device virtio-net,netdev=vmnic -netdev user,id=vmnic
```

After VM boot login with previously set root password (or another user if created).
To mount source image run 

```bash
mkdir /mnt/sources/
mount /dev/sdb /mnt/sources
```


# Optional: SMP configuration
To properly passthrough CPU configuration to virtual machine provide topology in -smp parameter.

```bash
PROC=$(nproc)
SOCKETS=$(lscpu -J | jq -r '.lscpu[]| select(.field=="Socket(s):").data')
CORES=$(lscpu -J | jq -r '.lscpu[]| select(.field=="Core(s) per socket:").data')
TPERCORE=$(lscpu -J | jq -r '.lscpu[]| select(.field=="Thread(s) per core:").data')

-smp ${PROC},cores=${CORES},threads=${TPERCORE},sockets=${SOCKETS}
```

To mount the provided source image inside the QEMU run the following:
```bash
mkdir /mnt/src/
mount /dev/sdb /mnt/src/
```
