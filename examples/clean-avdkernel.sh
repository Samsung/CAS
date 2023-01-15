#!/bin/bash

# You would need to install clang-11 based toolchain (including ld.lld-11) to make this example working (or modify this example accordingly)

export ROOT_DIR=$(pwd)
export KERNEL_DIR=$ROOT_DIR/kernel
export ARCH=x86_64
export CLANG_TRIPLE=x86_64-linux-gnu-
export CROSS_COMPILE=x86_64-linux-gnu-
export LINUX_GCC_CROSS_COMPILE_PREBUILTS_BIN=${ROOT_DIR}/x86_64-linux-android-4.9/bin
DEVEXPS="CC=clang-11 LD=ld.lld-11 NM=llvm-nm-11 OBJCOPY=llvm-objcopy-11 DEPMOD=depmod DTC=dtc BRANCH=android12-5.10 LLVM=1 EXTRA_CMDS='' STOP_SHIP_TRACEPRINTK=1 DO_NOT_STRIP_MODULES=1 IN_KERNEL_MODULES=1 KMI_GENERATION=9 HERMETIC_TOOLCHAIN=${HERMETIC_TOOLCHAIN:-1} BUILD_INITRAMFS=1 LZ4_RAMDISK=1 LLVM_IAS=1 BUILD_GOLDFISH_DRIVERS=m BUILD_VIRTIO_WIFI=m BUILD_RTL8821CU=m"
(cd $KERNEL_DIR && make $DEVEXPS mrproper)
