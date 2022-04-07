#!/bin/bash

# You would need to install clang-11 based toolchain (including ld.lld-11) to make this example working (or modify this example accordingly)

export ROOT_DIR=$(pwd)
export KERNEL_DIR=$ROOT_DIR/kernel
export ARCH=x86_64
export CLANG_TRIPLE=x86_64-linux-gnu-
export CROSS_COMPILE=x86_64-linux-gnu-
export LINUX_GCC_CROSS_COMPILE_PREBUILTS_BIN=${ROOT_DIR}/x86_64-linux-android-4.9/bin
DEVEXPS="CC=clang-11 LD=ld.lld-11 NM=llvm-nm-11 OBJCOPY=llvm-objcopy-11 DEPMOD=depmod DTC=dtc BRANCH=android12-5.10 LLVM=1 EXTRA_CMDS='' STOP_SHIP_TRACEPRINTK=1 DO_NOT_STRIP_MODULES=1 IN_KERNEL_MODULES=1 KMI_GENERATION=9 HERMETIC_TOOLCHAIN=${HERMETIC_TOOLCHAIN:-1} BUILD_INITRAMFS=1 LZ4_RAMDISK=1 LLVM_IAS=1 BUILD_GOLDFISH_DRIVERS=m BUILD_VIRTIO_WIFI=m BUILD_RTL8821CU=m"
export KBUILD_BUILD_USER=build-user
export KBUILD_BUILD_HOST=build-host
export PATH=$LINUX_GCC_CROSS_COMPILE_PREBUILTS_BIN:$PATH
(cd $KERNEL_DIR && make $DEVEXPS mrproper)
(cd $KERNEL_DIR && KCONFIG_CONFIG=$KERNEL_DIR/.config $KERNEL_DIR/scripts/kconfig/merge_config.sh -m -r $KERNEL_DIR/arch/x86/configs/gki_defconfig ${ROOT_DIR}/common-modules/virtual-device/virtual_device.fragment)
(cd $KERNEL_DIR && ${KERNEL_DIR}/scripts/config --file .config -d LTO -d LTO_CLANG -d LTO_CLANG_FULL -d CFI -d CFI_PERMISSIVE -d CFI_CLANG)
(cd $KERNEL_DIR && make $DEVEXPS olddefconfig)
(cd $KERNEL_DIR && make $DEVEXPS -j32)
(cd $KERNEL_DIR && rm -rf staging && mkdir -p staging)
(cd $KERNEL_DIR && make $DEVEXPS "INSTALL_MOD_STRIP=1" INSTALL_MOD_PATH=$KERNEL_DIR/staging modules_install)
(cd $KERNEL_DIR && make -C $ROOT_DIR/common-modules/virtual-device M=../common-modules/virtual-device KERNEL_SRC=$KERNEL_DIR $DEVEXPS -j32 clean)
(cd $KERNEL_DIR && make -C $ROOT_DIR/common-modules/virtual-device M=../common-modules/virtual-device KERNEL_SRC=$KERNEL_DIR $DEVEXPS -j32)
(cd $KERNEL_DIR && make -C $ROOT_DIR/common-modules/virtual-device M=../common-modules/virtual-device KERNEL_SRC=$KERNEL_DIR $DEVEXPS -j32 "INSTALL_MOD_STRIP=1" INSTALL_MOD_PATH=$KERNEL_DIR/staging modules_install)
