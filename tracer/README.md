## Syscalls tracing kernel module
A linux kernel module for tracing selected syscalls. Inspired by https://github.com/ilammy/ftrace-hook.

### Build & Installation
1. Get packages needed for module build.
```bash
$ sudo apt install build-essential linux-headers-$(uname -r)
```
2. Build and install the module.
```bash
$ make
$ sudo make modules_install
```
Some kernel configurations may require modules to be signed. To do so, follow the instructions in [kernel module signing docs](https://www.kernel.org/doc/html/latest/admin-guide/module-signing.html).

### WSL build
1. Check kernel version:
```bash
$ uname -r
5.15.90.1-microsoft-standard-WSL2
```
2. Find and download proper kernel sources version from: [WSL2-Linux-Kernel](https://github.com/microsoft/WSL2-Linux-Kernel/tags)
```bash
$ wget https://github.com/microsoft/WSL2-Linux-Kernel/archive/refs/tags/linux-msft-wsl-5.15.90.1.tar.gz
$ tar -xzf linux-msft-wsl-5.15.90.1
$ cd WSL2-Linux-Kernel-linux-msft-wsl-5.15.90.1
```
3. Create directories for kernel modules
```bash
$ sudo mkdir -p /lib/modules/`uname -r`
$ ln -s `pwd` /lib/modules/`uname -r`/build
```
4. Compile kernel and prepare kernel build enviroment for compiling modules
```bash
$ zcat /proc/config.gz > Microsoft/current-config # This command gets the config of running kernel (it should be same as Microsoft/config-wsl)
$ sudo apt install make gcc git bc build-essential flex bison libssl-dev libelf-dev pahole
$ make KCONFIG_CONFIG=Microsoft/config-wsl -j $(nproc)
$ make KCONFIG_CONFIG=Microsoft/config-wsl prepare_modules
$ cp /sys/kernel/btf/vmlinux .
```
5. Proceed normal module compilation steps [Build & Installation](#build--installation)
6. Remember to mount debugfs before using etrace to be sure trace\_pipe exists
```bash
$ sudo mount -t debugfs none /sys/kernel/debug
```

### Usage
**Note**: commands described here are useful mostly for BAS developers. For end-user usage, see [README](../README.md) of main BAS project.

This module traces the whole process tree starting from a given root pid. To start the module, use insmod or modprobe:
```bash
# run from tracer directory
$ sudo insmod bas_tracer.ko root_pid=<pid>

# can be run from anywhere if "sudo make modules_install" was done
$ sudo modprobe bas_tracer root_pid=<pid>
```
specifying the process tree root pid in root_pid parameter. Currently, only pids from root namespace are supported.

Successful start is indicated in dmesg:
```
[  269.189775] bas_tracer: et_init called
[  269.190147] bas_tracer: Attaching to tracepoint: sys_enter
[  269.190689] bas_tracer: Attaching to tracepoint: sys_exit
[  269.191225] bas_tracer: Attaching to tracepoint: sched_process_exit
[  269.191830] bas_tracer: Attaching to tracepoint: sched_process_fork
[  269.192440] bas_tracer: Attaching to tracepoint: sched_process_exec
[  269.193142] bas_tracer: Module loaded

```

To remove the module use rmmod:
```bash
$ sudo rmmod bas_tracer
```

The module puts data about traced processes into trace_pipe:
```bash
$ sudo cat /sys/kernel/debug/tracing/trace_pipe
```
to track per-cpu buffers:
```bash
$ sudo cat /sys/kernel/debug/tracing/per_cpu/cpu0/trace_pipe
$ sudo cat /sys/kernel/debug/tracing/per_cpu/cpu1/trace_pipe
# etc...
```
Example output:
```
0: 6497,31,1586953334,420390722!New_proc|argsize=31,prognameisize=7,prognamepsize=7,cwdsize=35
0: 6497,31,1586953334,420392205!PI|/bin/sh
0: 6497,31,1586953334,420392956!PP|/bin/sh
0: 6497,31,1586953334,420393557!CW|/media/storage/tools/execve_tracing
0: 6497,31,1586953334,420396052!A[0]sh
0: 6497,31,1586953334,420397364!A[1]/usr/bin/setup_etrace.sh
0: 6497,31,1586953334,420398486!A[2]-r
0: 6497,31,1586953334,420399118!End_of_args|
0: 6496,60,1586953334,420415598!Close|fd=5
0: 6497,31,1586953334,420435295!Open|fnamesize=16,flags=524288,mode=0,fd=3
0: 6497,31,1586953334,420436137!FN|/etc/ld.so.cache
0: 6497,31,1586953334,420438701!Close|fd=3
```

The description of the format used by tracer is available in [OUTPUT.md](./OUTPUT.md).
