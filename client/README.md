# About

This is client application CAS database.

# Requirements

## CAS libetrace library

CAS Client requires built libetrace.so file. Please check "Building CAS libraries" in main [README.md](../README.md).

# Usage

CAS Client can be run from commandline using `cas` command located in main repo directory.

Running `cas` without any arguments will print command help.

Bash completeion may be a great help when using cas - check [Bash completion](#bash-completion).

CAS Client requires database processed from CAS Tracer.
CAS Database contains information about opened files and process executed during tracing.

# Modules

There are several `modules` that can be used to get CAS Database information.

### Database creation commands
| module | command(s) | Used for  |
| ------------- | ------------- | ------------- |
| Parse | `parse` | Parse trace file `.nfsdb` to intermediate format |
| Post process | `postprocess` , `pp` | Create information about compilations,  |
| Create cache | `cache` | Stores post-process information to output image `.nfsdb.img` |

### Query commands
| module | command(s) | Returns |
| ------------- | ------------- | ------------- |
| Linked modules | `linked_modules` , `lm` , `m` | Files created by linker process |
| Binaries | `binaries` , `bins` , `b` | Executed binaries paths |
| Commands | `commands` , `cmds` | Executed commands |
| Compiled | `compiled` , `cc` | Compiled files |
| Reverse compilation dependencies  | `revcomps_for`, `rcomps` | Modules that depends on compiled file |
| Compilation info | `compilation_info_for` , `ci` | Compilation info of given file path |
| Referenced files | `ref_files` , `refs` | Files referenced during build|
| Process referencing file  | `procref` , `pr` | Files referenced by specific process |
| File access  | `faccess` , `fa`| Process referencing given file |
| File dependencies  | `deps_for` , `deps` | Dependencies of given file |
| Modules dependencies  | `moddeps_for` | Dependencies of given linked module |
| Reverse modules dependencies  | `revmoddeps_for` | Given module reverse dependencies |

### Database information commands
| module | command(s) | Returns |
| ------------- | ------------- | ------------- |
| Compiler patterns  | `compiler_pattern` | Patterns used to categorize executables as compilers |
| Linker patterns  | `linker_pattern` | Patterns used to categorize executables as linkers |
| Root process pid  | `root_pid` , `rpid` | Process id of main tracing process |
| Database version  | `version` , `ver` , `dbversion` | Database version (set in `cache` creation) |
| Source root  | `source_root` , `sroot` | Starting directory - used to calculate relative location |
| Config  | `config` , `cfg` | Config file (`.bas_config`) used in database generation |
| Stats  | `stat` | Database statistics |

# CAS Database creation 

Before using [query commands](#query-commands) user must parse and process `.nfsdb` trace file using [database creation commands](#database-creation-commands).

Typical case of building database looks like this:
1) Parse
    ```
    cd /directory/with/tracefile/

    cas parse
    ```
    This step uses `.nfsdb` file and produce intermediate `.nfsdb.img.tmp` file. 

2) Post process
    ```
    cas post-process --set-root=/directory/where/tracing/started
    ```
    This step uses `.nfsdb.img.tmp` and produce a lot of information:
    - compilations info - `.nfsdb.comps.json`
    - reversed binary mapping - `.nfsdb.rbm.json`
    - precompiled command patterns - `.nfsdb.pcp.json`
    - linked files info - `.nfsdb.link.json`

    At the end of postprocessing information from `.nfsdb.img.tmp` are merged with the above files and written to `.nfsdb.json`.

3) Build database images
    ```
    cas cache --set-root=/directory/where/tracing/started --set-version=dbver_1.0 
    ```
    This step uses `.nfsdb.json` and produces `.nfsdb.img` optimized memory-mapping database.


# Filtering

All query commands can use filtering. Filters can use `and` and `or` logical expressions between filter parts. Each filter part can be negated.

There are two types of filters - `file` and `exec` filters. Each have different set of keywords.

Filter type depends on context of returned information - eg `linked_modules` module will use `file` filter , `commands` will use `executable` filter. There is exception of `--commands` parameter, when this is set both filters can be used.

## Usage

    --filter=[keyword=val,[:<keyword=val>]]and/or[]... 

## Examples

    --filter=[source_root=1,exists=1]
    --filter=[path=*vmlinux.o,type=wc,exists=1]
    --filter=[path=/usr/bin/*,type=wc]or[exist=1]and[cmd=/bin/bash]
    --filter=[path=*clang*,type=wc,class=compiler]
    --filter=[bin=*prebuilts*javac*,type=wc]
    --filter=[path=*.c,type=wc,exists=1,access=r]
    --filter=[path=.*\\.c|.*\\.h,type=re]

## Filter keywords

**`class=<class_opt>`** - filters returned values based on file or executable category.

      <class_opt>:
          linked
          linked_static
          linked_shared
          linked_exe
          compiled
          plain
          compiler
          linker

**`path=<file_path>`** - specify file path (applies to `file` filters)

**`cwd=<cwd_dir>`** - specify cwd dir filter (applies to `execs` filter)

**`bin=<bin_path>`** - specify executable bin path filter(applies to `execs` filter)

**`cmd=<cmd>`** - specify command line filter (applies to `execs` filter)

**`type=<type_opt>`** - specify type of "path", "cwd", "bin", "cmd" parameters.

      <type_opt>:
          re   -->  regular expression
          wc   -->  wildcard
          ex   -->  simple partial match - default if no type is provided

**`ppid=<process_pid>`** - specify process parent pid (applies to `execs` filter)

**`access=<access_opt>`** - specify file access method (applies to `file` filter)

      <access_opt>:
          r    -->  only read
          w    -->  only written
          rw   -->  read and written

**`exists=<exists_opt>`** - specify file presence at the time of database generation (applies to `file` filter)

      <exists_opt>:
          0    -->  file does not exists
          1    -->  file exists
          2    -->  directory exists

**`link=<link_opt>`** - specify if file is symlink (applies to `file` filter)

      <link_opt>:
          0    -->  only read
          1    -->  only read

**`source_root=<source_root_opt>`** - specify if file or binary is located in source root

      <source_root_opt>:
          0    -->  only read
          1    -->  only read

**`source_type=<source_type_opt>`** - specify compiled file type (applies to `file` filter)

      <source_type_opt>:
          c
          c++
          other

**`negate=<negate_opt>`** - negate filter

      <negate_opt>:
          0    -->  normal filter
          1    -->  negate filter

# Extended path argument

Dependency generation steps can use extended path arguments.

## Usage

    --path=[keyword=val,[:<keyword=val>]]

## Keywords

* **`file=<file_path>`** 
* **`direct=<true|false>`** 
* **`exclude_cmd=<exclude_cmd>`** 
* **`exclude=<excl_patterns>`** 
* **`negate_pattern=<true|false>`** 

## Examples

    --path=[file=/abcd,direct=true,exclude=*.ko]

# Bash completion
There is handy bash completion script in `tools/bash_completion` dir. 

## Setup
Add "source" entry of `tools/bash_completion/cas_completion.sh` file in your ~/.bash_completion.

Example of `~/.bash_completion` file:
```bash 
. /home/j.doe/DEV/CAS/tools/bash_completion/cas_completion.sh
```

## Verify
After re-login run following command to verify:
```bash
:~/DEV/CAS$ complete | grep cas_completions
complete -F _cas_completions cas
```
If `complete -F _cas_completions cas` appears it means that `cas` commanline tool should display hints on double `<TAB>` keypress.
