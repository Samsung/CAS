## Build Awareness Service

BAS is a tool for extracting information regarding executed processes and opened files from ongoing builds. It uses specialized kernel module that takes advantage of the Linux kernel tracing infrastructure (i.e. `ftrace`) to grab this information with minimal overhead. The original file produced by the tracer is `.nfsdb`. It contains a raw sequence of syscall information in some predefined format (described in the tracer [readme](../tracer/README.md) file). Next processing stage is the `etrace_parser` application. It parses the .nfsdb file and produces JSON output (`.nfsdb.json` file by default).

  ```bash
  $ etrace_parser .nfsdb .nfsdb.json
  ```

  Entries in parsed `.nfsdb.json` file describe single program execution (i.e. events around single execve syscall) and contain processed and combined information from variosu syscall events.

  The tracer tracks (and the raw output JSON contains information about) the following events:

  - fork [class "f"]

    This describes fork and clone syscalls

  - exit [class "x"]

    This describes process exit

  - exec [class "e"]

    This describes execution of binary (script) within a process (execve and execveat syscalls)

  - open [class "o"]

    This describes plain open/openat syscalls

  - close [class "c"]

    This describes close syscall on file descriptor

  - pipe [class "p"]

    This describes a family of pipe syscalls (pipe, pipe2)

  - dup [class "d"]

    This describes a family of dup syscalls (dup, dup2, dup3)

  - rename [class "r"]

    This describes rename family of syscalls (rename, renameat, renameat2) 

  - link [class "l"]

    This describes link family of syscalls (link, linkat)

  - symlink [class "s"]

    This describes the symlink family of syscalls (symlink, symlinkat)

  The parsed JSON output entries have the following format:

  ```
  [
    {
      "p": <pid>,
      "x": <exeidx>, // index of program execution in this process (as one process can have multiple program executions through exec family syscalls)
      "s": <stime>,  // start time (in nanoseconds) from the first event for this specific execution
      "e": <etime>,  // elapsed time (in nanoseconds) for this specific execution
      "r": {"p": <pid>, "x": <exeidx>}, // parent entry (pid==-1 means this is the root process execution)
      "c": [{"p":<pid>,"f": <flags>},...], // list of child processes spawned from this execution
      "b": "<path_to_executed_binary>",
      "w": "<cwd>",
      "v": ["<argv>",...], // list of program execution command line (argv)
      "!": <status>, // exit status of this process, appears only on last execution of a given process
      "o": [{"p":"<path>","o":<original path>,"m":<mode>,"s":<size>},...],
        // list of opened files in this execution ('original_path' entry is only present when it differs from the 'path' entry)
        // 'size' entry is only meaningful if the mode indicates that the file exists after the build
      "n": "<base64 pcp array>", // precomputed command patterns for this execution (more info below)
      "m": <wrapper_pid>, // reverse binary mapping wrapper pid (more info below)
      "i": [{"p": <pid>, "x": <exeidx>},...] // list of executions that could read data from this execution through pipe
      "d": {  // compilation information by this compiler execution (exists only if this execution was a compilation)
             "f": ["<compiled_file>",...], // paths to the files compiled by this compiler process
             "i": ["<include_path0>","<include_path1>",...], // list of include paths used in this compilation
             "d": [{"n":"<N>","v":"<V>"},{},...], // list of preprocessor definitions from the command line
             "u": [{"n":"<N>","v":"<V>"},{},...], // list of preprocessor undefs from the command line
             "h": ["<header_path>","<header_path>",...], // list of files included at the command line
             "s": 1 or 2, // c compilation (1) or C++ compilation (2)
             "o": ["<obj0>","<obj1>",...], // list of object files crated by this compiler process
             "p": <is_not_integrated_compiler> // this is 1 if the driver compiler invocation is in the parent process
                                               // otherwise it is 0 (integrated compiler)
      },
      "l": <path_to_linked_file_created_by_this_linker_process> // exists only if this execution was a linker
      "t": <type of the linked module>, // 1 for the really linked executable/shared object and 0 for the ar archive
      "u": [{"t": <timestamp>,"c":<cpu_id>}] // ids of the cpus the process was running on with timestamps of the first event on that cpu
    },
    (...)
  ]
  ```

  Open mode bits are interpreted as follows: `0emmmmxx` where:
  - `e` bit is 1 if file exists during parsing events (and the `mmmm` bits are meaningful only if `e` bit is 1)
  - `mmmm` bits represent the file stat mode, i.e.:
  ```
  0x8: regular file
  0x4: directory
  0x2: character device
  0x6: block device
  0xA: symbolic link
  0x1: FIFO (named pipe)
  0xC: socket
  ```
  - `xx` bits describe the open mode of a file, i.e.
  ```
  0x0: read only
  0x1: write only
  0x2: read/write mode
  ```
  When file is opened multiple times in a process the open mode gets read/write whenever two distinct opens differ otherwise it is unchanged (i.e. a series of read only opens results in read only open mode in `mode` entry). When file is opened multiple times in a process the file stat mode of the last open is preserved.

  Sometimes the path passed to the `open` syscall differs from the path that is ultimately operated (for example symbolic link is passed to `open`). In such cases the `original_path` field will be populated with the value of the original `open` argument (and resolved path will be stored in `path`).

  Originally the parsed entries don't include the `"n"`, `"m"`, `"d"`, and `"l"` entries. These are produced and later added to the `.nfsdb.json` file in the post-processing step (more on that below).

  Environmental variables which of traced processes are outputted to a `.nfsdb.env.json` file of following format:
  ```json
  {
      "foo=bar": [pid1, pid2, pid3],
      "bar=baz": [pid4, pid5, pid6]
  }
  ```

  Part of the BAS suite is also the support library: **libetrace.so**. This is the Python binding library that contains native implementation of selected functions. Currently it contains the code for processing compilations as well as dependency processing functions. It also contains Python bindings for operating on cached version of the database.

  In the Python code use:

  ```python
  import libetrace
  libetrace.<make_your_call_here> # For module wide available functions
  libetrace.nfsdb() # To create the base object for cached database operation
  ```
  
  Please keep in mind that the `PYTHONPATH` variable should be pointing to the directory that contains the `libetrace.so` file (i.e the CAS root directory where the library is installed).
  
  Other parts of BAS suite are listed below.

* **libgcc_input_name.so**

  This is a helper gcc plugin for extracting source file name from the command line of gcc invocation. This requires `gcc-plugin-dev` module installed in the OS.

* **etrace_cat**

  This is a simple cat utility with slight modification to exit the reading/writing loop whenever SIGINT signal is send to the process. The `cat` implementation was originally taken from the CoreUtils package.

* Postprocessing phase

  The postprocessing phase adds missing information to `.nfsdb.json`, like data about linked modules, compilations, precomputed command patterns and reverse binary mappings. It updates `.nfsdb.json` with additional entries (`"n"`, `"m"`, `"d"` and `"l"`).
  Previously, we used a separate `postprocess.py` script to do that, nowadays this is done by the CAS client:
  ```sh
  $ cas pp
  ```

The post-processing stage performs some additional steps:
* extracting a list of linked/ared modules from all the exec events and updating database entries;
* analyzing all compilations from the build and updating the database entries with extracted information;
* computing reverse binary mappings for specific binaries;
* computing precomputed command patterns for specific string queries.

One of the post-processing steps is to produce reverse binary mappings for specific binaries. Sometimes executed processes are intertwined together, for example:
  ```shell
  /bin/bash -c "cat <...> | sort > out.f"
  ```
  When we track files written by the bash command above it turns out that the actual writing is done in the `sort` command which receives the data through the pipe. In this case instead of considering the `sort` process we want to consider the wrapping `/bin/bash` call. We do that by creating reverse binary mapping for `/bin/bash` command which maps all descendant processes back to their wrapping `/bin/bash` execution. We can then act as the real writing process was the wrapping `/bin/bash`.

  The core functionality of the Build Awareness Service is to compute file dependencies for selected files. For example we would want to compute all the file dependencies for the `vmlinux` linked Linux kernel image (i.e. all source and header files (among other things) this file depends on). Computing file dependencies for a given file based only on file open information is error prone. Consider the following situation:
  ```bash
  ld -o myexe -T my_script.lds obj1.o obj2.o obj3.o <...>
  ```
When file dependencies are computed for the `myexe` file, first we will look for processes that have written to the `myexe` file (most probably we will find only the `ld` linker program). Then we establish a list of files read by the `ld` process (i.e. `my_script.lds` and all the object files). Then for each file in the dependencies of `ld` which were open for write (in most cases all object files will satisfy that) we are looking for processes who wrote to those files (in our case these should be compiler processes that created the object files). In another step for each such process we establish a list of files read by any of them. This process goes on until we cannot find any written file in the dependencies (we assume we've reached the underlying source files). But what if the `my_script.lds` was autogenerated itself? Let assume as well it was generated by the main build script for a complicated source tree (let's call it `build.sh`). The main build script probably does a bunch of other things and reads a lot of files along the way (and many of them are writable). We can quickly finish up with very large number of dependencies for the `myexe` binary and most of them are false positives. We need some sort of filtering mechanism for dependency processing.

There are two mechanisms for constraining the dependency computation: excluding file patterns and exluding command patterns. In the first case when the file in the read dependencies of some process matches the file exclusion pattern it is not considered further even though it might be open for writing as well. The second mechanism is that when the program execution command line matches the exclusion pattern for the considered process we will not process further any files read by it. Therefore when the following command exclusion pattern is specifiec in configuration file `<path_to>/build.sh*` all further dependencies for the `my_script.lds` file that comes from the `build.sh` command will be omitted.

The process of dependency processing for a given file can be very complex and thousands of files and processes can be considered. Also the program command lines that need to be matched with multiple patterns can be very long. All of this can decrease the process of dependency computation significantly. Keeping in mind that in complex build we might have several thousand linked files potentially to establish file dependencies serious optimization is definitely required. 

One might notice that the number of executed programs and number of patterns is well known information and can be precomputed and stored in the database. This is exactly what precomputing command patterns step does. It will precompute all the command exclusion patterns across all executed programs and save the information in `"n"` entries of the parsed entries in `.nfsdb.json` file).
