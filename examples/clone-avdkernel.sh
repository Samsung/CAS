#!/bin/bash

# Download Linux kernel for the emulator so we could spin some examples

mkdir -p avdkernel
(cd avdkernel && git clone https://android.googlesource.com/kernel/build build)
(cd avdkernel && git clone https://android.googlesource.com/platform/prebuilts/build-tools prebuilts/build-tools)
(cd avdkernel && git clone -b android-11.0.0_r28 --single-branch https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/x86/x86_64-linux-android-4.9)
(cd avdkernel && git clone https://android.googlesource.com/kernel/prebuilts/build-tools prebuilts/kernel-build-tools)
(cd avdkernel && git clone https://android.googlesource.com/kernel/common-modules/virtual-device common-modules/virtual-device)
(cd avdkernel/common-modules/virtual-device && git checkout 272a06c7d90c63f756ee998957609c25ebc6a6cf)
(cd avdkernel && git clone https://android.googlesource.com/kernel/common kernel)
(cd avdkernel/kernel && git checkout 53a812c6bbf3d88187f5f31a09b5499afc2930fb)
