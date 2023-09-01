## Code Aware Services

Code Aware Services (CAS) is a set of tools for extracting information from a (especially large) source code trees. It consists of Build Awareness Service (BAS) and Function/Type database (FTDB). BAS is a tool for extracting information how particular S/W image is created from ongoing builds. FTDB transforms predefined source code information (like information about functions and types) into easily accessible format (like JSON) which can be used by a number of applications.

### Introduction

Imagine you need to fix a S/W issue in a large S/W product (like Mobile Phone). This kind of project usually consists of a large number of S/W modules (like kernel, modem, system libraries and applications) communicating with each other. You need to make a general overview of the source code structure to find interesting source files and identify dependencies between modules. You also need detailed information how the S/W was built (i.e. exact compilation switches for specific source files) to be able to locate S/W errors that depend on specific configuration.

BAS works by tracing the full S/W image build (using specialized Linux kernel module) and storing information about all opened files and executed processes. This is later post-processed to extract detailed compilation information from compilation processes. All that information is stored in database (JSON file) which can be accessed freely after the build.

Examples of potential uses of BAS are:

* extracting the list of all files from entire source tree which were used to create the final S/W image (which can be much smaller number (like ~10% in case of Linux kernel build) than the entire source repository) which can radically improve code search tools;

* generating compilation database (i.e. JSON file with all compilation switches used to build the final S/W image) which can be later used by other tools operating on source code, like static analyzers, source code transformation tools, custom source processors etc.;

* generating custom makefiles that can incrementally rebuild specifically selected parts of the source tree which can speed-up the change/build/test iteration during S/W development

* generating code description files to various IDEs (like Eclipse IDE) which can significantly improve development process for user space applications (or OS kernel)

* analyzing build related information to locate S/W errors

and more.

Now imagine that you want to write a tool that operates on source code, i.e. custom static analyzer which looks for specific erroneous patterns in the code and informs the developer about potential issues to focus on. Writing such tool requires a parsed source code representation to be available but writing parsers (especially for complex languages like C++) is extremely difficult. FTDB is a tool that uses existing libraries (i.e. libclang library) to parse the source code and store selected information from the parsed representation in an easily accessible format (i.e. JSON database). Such database can be later used by applications (i.e. Python programs) which can implement a set of distinct functionalities that takes source code information as an input.

Together BAS with FTDB forms a tightly related toolset which was collectively named CAS (Code Aware Services).


### Details

CAS is split into three separate functionalities (stored in three separate source directories):
* build tracer ([tracer](tracer) directory)

This is the Linux kernel module for tracing specific syscalls executed during build. The module file produced is **bas_tracer.ko**. More details about tracer operation can be found in [readme](tracer/README.md) file.

* BAS tools ([bas](bas) directory)

This is a set of user space tools used to process and operate on the trace file produced by the build tracer. More details can be found in [readme](bas/README.md) file.


* FTDB processor ([clang-proc](clang-proc) directory)

This is the clang processor used to parse source files (currently only C sources are supported (but C++, at least partiallyl is on its way)) to extract relevant information from the parsed representation and store it in simple JSON database file. More details can be found in [readme](clang-proc/README.md) file.

### How to build

First setup the build environment and download sources:
```bash
sudo apt install git cmake llvm clang libclang-dev python3-dev gcc-9-plugin-dev build-essential linux-headers-$(uname -r) python2.7-numpy python-futures flex bison libssl-dev
git clone https://github.com/Samsung/CAS.git && cd CAS
export CAS_DIR=$(pwd)
```

You can build all components using any decent compiler however for FTDB processor to work you'll need `llvm` and `clang` in version at least 10 (up to 13). In standard Ubuntu 20.04 distribution the above command will install this version by default. In case of other OS arrangements please make sure that the `llvm-config --version` returns the proper supported number. Alternatively you can pass `-DLLVM_CONFIG_BIN=llvm-config-1x` to cmake invocation to properly setup the clang processor based on clang 1x libraries.

Building the tracer:

```bash
(cd ${CAS_DIR}/tracer && make && sudo make modules_install)
```

Some kernel configurations may require modules to be signed. To do so, follow the instructions in [kernel module signing docs](https://www.kernel.org/doc/html/latest/admin-guide/module-signing.html).

After the tracer module is installed setup the tracing wrapper:
```bash
(cd ${CAS_DIR} && sudo ./etrace_install.sh)
```

This will modify your sudoers file to allow for using the tracer kernel module by any user in the system.

Building CAS libraries:
```bash
mkdir -p ${CAS_DIR}/build_release && cd ${CAS_DIR}/build_release
# This will configure to build with standard system compiler 'cc'
cmake -DCMAKE_BUILD_TYPE=Release ..
# Use this if you want to use clang explicitly
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ..
make -j16 && make install
```

### How to use

**etrace** - this is the wrapper script that runs the command (most probably build) under the tracer.

```bash
export PATH=${CAS_DIR}:$PATH
# Run the COMMAND and save tracing log file (.nfsdb) in the current directory
$ etrace <COMMAND>
# Run the COMMAND and save tracing log file (.nfsdb) in the WORK_DIR directory
$ etrace -w <WORK_DIR> <COMMAND>
# Run the COMMAND and save tracing log file (.nfsdb) in the current directory (also keep kernel log file along the way)
$ etrace -l <COMMAND>
# Run the COMMAND and save tracing log file (.nfsdb) in the WORK_DIR directory (also keep kernel log file along the way)
$ etrace -w <WORK_DIR> -l <COMMAND>
```

  All the above commands also produce `.nfsdb.stats` file. These are the statistics from the underlying ftrace infrastructure. Most common usage is to check for possible dropped events (meaning the underlying kernel buffer should be larger):

  ```bash
  $ cat .nfsdb.stats | grep dropped
  ```

  The `-l` option saves the kernel log file (dmesg) during execution to the `.nfsdb.klog` file. This can be used for debugging of the tracer kernel module.

  Beware, whole kernel log file is saved (even events before the execution of the tracer). To get rid of those use `sudo dmesg -C` before running the tracer.

**etrace_parser** - this will parse the tracing log file (.nfsdb) and produce the build information in the form of simple JSON file (for the description of JSON format please refer to the [readme](bas/README.md) file).

**cas** - this is the command line tool that controls various CAS suite operations. It can create the BAS database or query it for specific data. It provides high level functionalities that takes the BAS database as a source of data.

To parse the tracing log file navigate to the directory where the `.nfsdb` file is located and simply execute the following:
```bash
cas parse
```
The result is written into `.nfsdb.json` file.

Another step is to post-process the `.nfsdb.json` file and include additional information regarding compilations and other.

```bash
cas postprocess
```

If your first-stage parsing `.nfsdb.json` is located in different directory than the root of your source tree you can provide the source root information using the `--set-root` option:
```bash
cas postprocess --set-root=<path to source root directory>
```

There are times when the post-processed JSON file is really large (in case of full Android builds for latest OS version it can easily take few GB of size). Reading such JSON can be very time consuming and take a few minutes just to parse it. There is option to create a specialized cached version of the JSON file strictly dedicated for reading only. This will also (try to) precompute the file dependency information for all linked modules created during the build and save it into the (separate) cache file.

```bash
cas cache [--set-version=<version string>] [--set-root=<path to source root directory>] [--shared-argvs=<shared linking spec>] [--jobs=<int>]
```

This will produce the `.nfsdb.img` file with in-memory representation of the parsed JSON data which can be read back into memory almost instantly.

The shared linking spec provides a list (delimited by `:`) of linker switches (that were used during the build) used to tell a linker that it should produce a shared library. For most of the possible usages it should be enough to provide `-shared:--shared` values (it's actually the default value).

The number of jobs used during the cache creation specifies how many processors should be used for the file dependency computation of the linked modules (which defaults to the number of cores available in the system).

For more information regarding the `cas` command line tool (and other clients to access the database content) please see [client readme](CLIENT.md) file.

### Examples

Let's see some quick introduction through several simple examples. Let's build our CAS suite again but this time with the tracer enabled.

```bash
cd ${CAS_DIR}/build_release
make clean
etrace make -j16 && make install
cas parse
cas postprocess --set-root=${CAS_DIR}
cas cache --set-root=${CAS_DIR}
```
 
Now strike up some editor and write-up the following Python code:
```python
#!/usr/bin/env python3

import sys
import libetrace

nfsdb = libetrace.nfsdb()
nfsdb.load(sys.argv[1],quiet=True)

print ("Source root of the build: %s"%(nfsdb.source_root))
print ("Database version: %s"%(nfsdb.dbversion))

# List all linked modules
L = [x for x in nfsdb if x.is_linking()]
print ("# Linked modules")
for x in L:
    print ("  %s"%(x.linked_path))
print()

# List compiled files
C = [x for x in nfsdb if x.has_compilations()]
print ("# Compiled files")
for x in C:
    for u in x.compilation_info.file_paths:
        print ("  %s"%(u))
print()

# List all compilation commands
print ("# Compilation commands")
for x in C:
    print ("$ %s\n"%(" ".join(x.argv)))
```

Or try existing example:
```bash
export PYTHONPATH=${CAS_DIR}:$PYTHONPATH
${CAS_DIR}/examples/etrace_show_info .nfsdb.img
```

This should give you a list of all linked modules, compiled files and compilation commands used during the CAS suite build.

We can generate FTDB database for all C compiled files.
```bash
export CLANG_PROC=${CAS_DIR}/clang-proc/clang-proc
${CAS_DIR}/examples/extract-cas-info-for-ftdb .nfsdb.img
${CAS_DIR}/clang-proc/create_json_db -P $CLANG_PROC
${CAS_DIR}/tests/ftdb_cache_test --only-ftdb-create db.json
```

First command extracts the compilation database (i.e. list of all compilation commands used to compile the program which is later used by the clang processor). Second command creates the FTDB database (`db.json` file in our case). Finally we create the cached version of the FTDB database (you can perfectly use the original database `db.json` file if that suits you however as soon will be shown this JSON file can also get really large very quickly for complex modules).

Some simple information regarding the FTDB database:
```bash
${CAS_DIR}/examples/ftdb_show_info db.img 
```

Ok, let's see some more serious example now. First clone Linux kernel source for AOSP version 12 (make sure you have a few GB of space available).

```bash
cd ${CAS_DIR}
examples/clone-avdkernel.sh
```

Sources should be downloaded to `avdkernel` directory. To build it you'll need clang at least in version 11.
```bash
sudo apt-get install clang-11 llvm-11 lld-11
cd avdkernel
etrace ${CAS_DIR}/examples/build-avdkernel.sh
cas parse
cas postprocess
cas cache
```

We will now extract information required to create the FTDB database for the Linux kernel (and all accompanying modules). This time we will get some more information as we will build specialized version of FTDB suited for the automated off-target generation (please see [AoT](https://github.com/Samsung/auto_off_target) project for more details).

```bash
${CAS_DIR}/examples/extract-avdkernel-info-for-ftdb .nfsdb.img
```

You should see confirmation that three files were created along the way. Apart from compilation database we also extract compilation dependency map (i.e. list of compiled files for every module of interest) and reverse dependency map (i.e. for every source file we get information which module used that file).
```
created compilation database file (compile_commands.json)
created compilation dependency map file (cdm.json)
created reverse dependency map file (rdm.json)
```

Now you're ready for FTDB generation.
```bash
${CAS_DIR}/clang-proc/create_json_db -P ${CLANG_PROC} -p fops -o fops.json
${CAS_DIR}/clang-proc/create_json_db -p db -o db.json -P $CLANG_PROC -F fops.json -m android_common_kernel -V "12.0.0_r15-eng" -mr ${CAS_DIR}/clang-proc/vmlinux-macro_replacement.json -A -DD ${CAS_DIR}/clang-proc/vmlinux-additional_defs.json -cdm cdm.json -j4
```

First we create some intermediate file, `fops` database. For every global variable of structure type (i.e. `struct file_operations`) that contains some function pointer members (i.e. `ioctl`) which are initialized statically we grab (and save) the function names that initialize them. Second invocation crates the final `db.json` file (with some additions helpful for the `AoT` project). If you have infinite amounts of RAM you can skip the `-j4` option and it'll run on full speed. Otherwise I suggest to keep it that way.

The JSON database reached a few GB of size. Now it's fully justified to use the cache.
```bash
${CAS_DIR}/tests/ftdb_cache_test --only-ftdb-create db.json
${CAS_DIR}/examples/ftdb_show_info db.img
```

Now the FTDB database can be used for further analysis and application development.


### Running in docker

Running in docker environment requires some extended capabilities so running with --privileged switch is recommended. Also --pid="host" is required for proper capture of tracing data from mounted `/sys/kernel/debug`. 

Container should enable user to load kernel module from host so `/lib/modules/` must be mounted and `kmod` system package are required.

Example of Ubuntu 22.04 usage:

```
docker run -it --pid="host" --privileged -v /lib/modules/:/lib/modules/ -v /sys/kernel/debug:/sys/kernel/debug -v ${CAS_DIR}:/cas ubuntu:22.04

// Setup
root@5f01ed8691a8:/# apt update
root@5f01ed8691a8:/# apt install kmod
root@5f01ed8691a8:/# cd /cas/
root@5f01ed8691a8:/cas# ./etrace_install.sh
// Testing
root@5f01ed8691a8:/cas# cd /tmp/
root@5f01ed8691a8:/tmp# /cas/etrace ls /
// Verify
root@5f01ed8691a8:/tmp# ll /tmp/
total 20
drwxrwxrwt 1 root root 4096 Jan 10 09:46 ./
drwxr-xr-x 1 root root 4096 Jan 10 09:46 ../
-rw-r--r-- 1 root root 8114 Jan 10 09:46 .nfsdb
-rw-r--r-- 1 root root 1363 Jan 10 09:46 .nfsdb.stats
```

Tracing is working fine if .nfsdb contains more than one line.

### Virtual Environment

For those who are cautious to load and run custom Linux kernel modules on their Linux machines there is also virtual environment (based on QEMU) provided. Please refer to the [readme](tools/virtual_environment/README.md) file.

### Talks

Talk about Code Aware Services from the Linux Security Summit NA 2022 can be found [here](https://youtu.be/M7gl7MFU_Bc?t=648)
