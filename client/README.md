# About

This is client application CAS database.

# Requirements

## CAS libetrace library

  When built locally access can be provided by adding build directory path to PYTHONPATH
eg.

  ```
  export PYTHONPATH=/home/j.doe/DEV/CAS/:$PYTHONPATH 
  ./cli.py
  ```


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
