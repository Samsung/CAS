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

Running CAS Clinet generally starts with using one of [modules](#modules) as argument.

    cas MODULE

Optional [filters](#filters) and [parameters](#parameters) can be added.

    cas MODULE filters* parameters*

CAS Client can use pipelines - which means that response from one module can be used as input to another.

    cas MODULE1 filters1* parameters1* MODULE2 filters2*

Output from CAS Client is depend on selected module and selected output renderer ([more about output renderers](#output-renderer)).

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
    This step uses `.nfsdb.img.tmp` and produce following files:
    - compilations info - `.nfsdb.comps.json`
    - reversed binary mapping - `.nfsdb.rbm.json`
    - precompiled command patterns - `.nfsdb.pcp.json`
    - linked files info - `.nfsdb.link.json`

    At the end of postprocessing executable data from `.nfsdb.img.tmp` is merged with files above and written to `.nfsdb.json`.

3) Build database images
    ```
    cas cache --set-root=/directory/where/tracing/started --set-version=dbver_1.0 
    ```
    This step uses `.nfsdb.json` and produces `.nfsdb.img` optimized memory-mapping database.


# Filters

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

**`class=<class_opt>`** - specify file or executable class.

    <class_opt>:
        // file class filters
        linked
        linked_static
        linked_shared
        linked_exe
        compiled
        plain
        // execs class filters
        compiler 
        linker 

**`path=<file_path>`** - specify file path (applies to `file` filters)

**`cwd=<cwd_dir>`** - specify executable current working directory filter (applies to `execs` filter)

**`bin=<bin_path>`** - specify executable bin path filter (applies to `execs` filter)

**`cmd=<cmd>`** - specify command line filter (applies to `execs` filter)

**`type=<type_opt>`** - specify type of "path", "cwd", "bin", "cmd" parameters.

    <type_opt>:
        sp   -->  simple partial match - default if no type is provided
        re   -->  regular expression
        wc   -->  wildcard

**`ppid=<process_pid>`** - specify pid of parent process (applies to `execs` filter)

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

# Parameters

Some parameters can be used multiple times (will be marked as `multi-parameter`) by invoking it multiple times or using `:` separator.
Example 
```
cas compilation_info_for --path=/file1 --path=/file2 --path=/file3:/file4
```

## Input parameters

**`--path=<paths>`** - Path parameter - used for most of `xx_for` modules. (`multi-parameter`)

        cas deps_for --path=/file1 --path=/file2

There is special case of `path` parameter that allows extended options - check [Extended path argument](#extended-path-argument)

**`--select=<paths> -S=<paths>`** - Select option forces return of file paths even if filters disallow them. (`multi-parameter`)

        cas linked_modules --filter=[path=/dev/null] --select=/file1 --select=/file2

**`--append=<paths> -a=<paths>`** - Append option forces return of file paths even if they don't exists in module results. (`multi-parameter`)

        cas linked_modules --append=/file1 --append=/file2

**`--pid=<pids>`** - Process id parameter. (`multi-parameter`)

        cas commands --pid=123 --pid=456

**`--binary=<paths> -b=<paths>`** - Binary parameter - used to get execs using provided binary path. (`multi-parameter`)

        cas bineries --binary=/file1 --binary=/file2        

## Return data modifiers

**`--commands --show-commands`** - Changes returned opens data to executables. 

**`--details -t`** - Returns more information about execs / opens.

**`--compilation-dependency-map --cdm`** - Returns compilation dependency map. 

**`--cdm-exclude-patterns`** - Exclude patterns for compilation-dependency-map

**`--cdm-exclude-files`** - Exclude files for compilation-dependency-map

**`--reverse-dependency-map --rdm`** - Returns reverse dependency map

**`--revdeps`** - Prints reverse dependencies

# Extended path argument

Dependency generation steps can use extended path arguments. This enable to pass extended parameters to each specified path.

## Usage

    --path=[keyword=val,[:<keyword=val>]]

## Keywords

* **`file=<file_path>`** 
* **`direct=<true|false>`** 
* **`exclude_cmd=<exclude_cmd>`** 
* **`exclude=<excl_patterns>`** 
* **`negate_pattern=<true|false>`** 

## Examples

    --path=[file=/abcd,direct=true,exclude=*.ko] --path=[file=/xyz,direct=false,exclude=*.a]

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

# FTDB creation
CAS client has a functionality to create ftdb by adding `--create-ftdb` on the end of `cas` query. Example usage:
```bash
    cas linked_modules --filter=<Filter> --create-ftdb
```
or
```bash
    cas deps_for --path=<Path> --create-ftdb
```
There are also additional parameters for ftdb creation.

| parameter | description | 
| ------------- | ------------- | 
| `--ftdb-output` | Path and name to output file for ftdb (.img extension) |
| `--version`  | Version of output FTDB image |
| `--macro-replacement-file` or `-mr`  | Path to definitions of macro replacement |
| `--additional-defines-file` or `-dd`  | Path to additional macro file for parsing |
| `--track-macro-expansions` or `-me`  | Path to definitions of macro replacement |
| `--compilation-dependency-map` or `-cdm`  | Switch for creating compilation dependency map and using it in ftdb creation (For modules output. Not dependencies) |
| `--ftdb-metaname` or `-m`  | Name of the output FTDB image |
| `--preserve-cta` | If used it will not merge compile time asserts into one declaration |
| `--make-unique-compile-commands` | Making sure that compile commands will be unique |

# Process Tree, Dependency Tree and Reversed Dependencies Tree
Process tree is showing children or parent processes. Clicking on a button with `<pid>:<idx>` will show children of the process. If we start the tree from other process than root, then we can also move up in the hierarchy by clicking `⇖` on the left. You can also start a new process tree by clicking the symbol `↸ ` at the end of bin path. Click letter `i` for more information about the process in proc_tree view.

Dependency tree is showing all direct module dependencies of the given file. If you want to see all the dependencies you can simply click letter `i` for more information. You can also start a new dependency tree by clicking the symbol `^` at the end of dependency path.
## From CAS
```bash
    cas <module> <filter> --commands=true --proc-tree/--deps-tree
```
## CAS Server
When you run `cas_server.py` there will be 3 endpoints `/deps_tree`, `/proc_tree` and `/revdeps_tree`. 
You can also add filters to create deps_tree/revdeps_tree for specified element or start proc_tree from specified element by adding `path=<path>` or `pid=<pid>` to query.
On the top right corner of the page is Search where you can find elements by path, type etc.