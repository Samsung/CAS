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
| Reverse compilation dependencies  | `revcomps_for`, `rcomps` |  |
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

```
cd /directory/with/tracefile/

cas parse
```
This step uses `.nfsdb` file and produce intermediate `.nfsdb.img.tmp` file. 

```
cas post-process
```
This step uses `.nfsdb.img.tmp` and produce a lot of information:
- compilations info - `.nfsdb.comps.json`
- reversed binary mapping - `.nfsdb.rbm.json`
- precompiled command patterns - `.nfsdb.pcp.json`
- linked files info - `.nfsdb.link.json`

At the end of post processing information from `.nfsdb.img.tmp` are merged with the above files and written to `.nfsdb.json`.

```
cas cache --set-version dbver_1.0
```
This step uses 


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

# Filtering

*TODO - write something about filters*
## Usage

    --filter=[keyword=val,[:<keyword=val>]]and/or[]... 

## Filter keywords

* **`class=<class_opt>`** - specify one of following entry class

      <class_opt>:
          linked
          linked_static
          linked_shared
          linked_exe
          compiled
          plain
          compiler
          linker

* **`type=<type_opt>`** - specify type of path-like pattern - applies to "path", "cwd", "bin", "cmd"

      <type_opt>:
          re
          wc

* **`path=<file_path>`** - specify path, wildcard or regular expression pattern (applies to file views)

* **`cwd=<cwd_dir>`** - specify cwd dir (applies to command views)

* **`bin=<bin_path>`** - specify command bin path (applies to command views)

* **`cmd=<cmd>`** - specify commandline (applies to command views) (*this one supprots only re pattern*)

* **`ppid=<process_pid>`** - specify process pid (applies to command views)

* **`access=<access_opt>`** - specify file access method (applies to file views)

      <access_opt>:
          r
          w
          rw

* **`exists=<exists_opt>`** - specify file presence at the time of database generation (applies to file views)

      <exists_opt>:
          0
          1
          2

* **`link=<link_opt>`** - specify if file is symlink (applies to file views)

      <link_opt>:
          0
          1

* **`source_root=<source_root_opt>`** - specify if file is located in source root (applies to file views)

      <source_root_opt>:
          0
          1

* **`source_type=<source_type_opt>`** - specify compiled file type (applies to command views)

      <source_type_opt>:
          c
          c++
          other

* **`negate=<negate_opt>`** - negate filter

      <negate_opt>:
          0
          1


## Examples

    --filter=[source_root=1,exists=1]

    --filter=[path=*vmlinux.o,type=wc,exists=1]

    --filter=[path=/usr/bin/*,type=wc]or[exist=1]and[cmd=/bin/bash]

    --filter=[path=*clang*,type=wc,class=compiler]

    --filter=[bin=*prebuilts*javac*,type=wc]

    --filter=[path=*.c,type=wc,exists=1,access=r]

    --filter=[path=.*\\.c|.*\\.h,type=re]

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
