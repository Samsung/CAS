## Syscalls tracing kernel module
A linux kernel module for tracing selected syscalls. Inspired by https://github.com/ilammy/ftrace-hook.

### Build & Installation:
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

### Usage
**Note**: commands described here are useful mostly for BAS developers. For end-user usage, see [README](../README.md) of main BAS project.

This module traces the whole process tree starting from a given root pid. To start the module, use insmod or modprobe:
```bash
# run from tracer directory
$ sudo insmod execve_trace.ko root_pid=<pid>

# can be run from anywhere if "sudo make modules_install" was done
$ sudo modprobe execve_trace root_pid=<pid>
```
specifying the process tree root pid in root_pid parameter. Currently, only pids from root namespace are supported.

Successful start is indicated in dmesg:
```
[ 1573.426731] exec_trace: et_init called
[ 1573.426733] exec_trace: Attaching to tracepoint: sys_enter
[ 1573.427357] exec_trace: Attaching to tracepoint: sys_exit
[ 1573.427794] exec_trace: Attaching to tracepoint: sched_process_exit
[ 1573.428207] exec_trace: Attaching to tracepoint: sched_process_fork
[ 1573.428621] exec_trace: Attaching to tracepoint: sched_process_exec
[ 1573.429076] exec_trace: Module loaded

```

To remove the module use rmmod:
```bash
$ sudo rmmod execve_trace
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
