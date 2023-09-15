# pylint: disable=missing-function-docstring missing-class-docstring too-many-lines missing-module-docstring 
import os
import fnmatch
import sys
import json
from typing import Iterator, Generator, List
import pytest
import timeout_decorator
import libcas

from client.cmdline import process_commandline
from client.argparser import get_api_modules, get_args, merge_args, get_bash_complete, get_api_keywords
from client.filtering import OpenFilter, CommandFilter, FilterException

DEF_TIMEOUT = 3
RAW_SEP = "^^^"

nfsdb = libcas.CASDatabase()
config = libcas.CASConfig(os.path.join(os.environ["DB_DIR"], ".bas_config"))
nfsdb.set_config(config)
nfsdb.load_db(os.path.join(os.environ["DB_DIR"], ".nfsdb.img"))
nfsdb.load_deps_db(os.path.join(os.environ["DB_DIR"], ".nfsdb.deps.img"))

comp_ext = {e.split("/")[-1].split(".")[-1] for e in nfsdb.get_compiled_file_paths()}
link_ext = {e.split("/")[-1].split(".")[-1] for e in nfsdb.linked_module_paths()}


@pytest.fixture(name="vmlinux")
def fixture_vmlinux() -> str:
    return get_main("linked_modules -n=1 --filter=[path=*vmlinux,type=wc,exists=1]and[compressed,negate=true]")


@pytest.fixture(name="vmlinux_o")
def fixture_vmlinux_o() -> str:
    return get_main("linked_modules -n=1 --filter=[path=*vmlinux.o,type=wc,exists=1]")


@pytest.fixture(name="some_obj")
def fixture_some_obj() -> str:
    return get_main("linked_modules -n=1 --filter=[path=*tmp_cpu.o,type=wc]")


@pytest.fixture(name="compiled_file")
def fixture_compiled_file() -> str:
    return get_main("compiled --filter=[path=*curl/lib/md5.c,type=wc] -n=1")


@pytest.fixture(name="header_file")
def fixture_header_file() -> str:
    return get_main("linked_modules --filter=[path=*libbtdevice.a,type=wc] deps_for --filter=[path=*android/packages/modules/Bluetooth/system/osi/include/list.h,type=wc]")


@pytest.fixture(name="javac")
def fixture_javac() -> str:
    return get_main("binaries --command-filter=[bin=*prebuilts/jdk/jdk11/linux-x86/bin/javac,type=wc] -n=1 --reverse")


@pytest.fixture(name="sh")
def fixture_sh() -> str:
    return get_main("binaries --command-filter=[bin=*bin/sh,type=wc] -n=1")


@pytest.fixture(name="bash")
def fixture_bash() -> str:
    return get_main("binaries --command-filter=[bin=*bin/bash,type=wc] -n=1")


@pytest.fixture(name="linker")
def fixture_linker() -> str:
    return get_main("binaries --command-filter=[bin=*/bin/ld.lld,type=wc] -n=1")


@pytest.fixture(name="pids")
def fixture_pids() -> list:
    return [int(x) for x in get_main("compiled --filter=[exists=1] --commands -n=3 --entry-fmt={p}").split(os.linesep)]


@pytest.fixture(name="ref_files_len")
def fixture_ref_files_len() -> int:
    return nfsdb.opens_num()

# @pytest.fixture(name="source_root")
# def fixture_source_root() -> str:
#     return get_main("source_root")


def in_debug() -> bool:
    return hasattr(sys, 'gettrace') and sys.gettrace() is not None


def get_json_entries(cmd, len_min=0, len_max=sys.maxsize, check_func=None, timeout=DEF_TIMEOUT):
    if "DB_DIR" in os.environ:
        cmd += " --dir={}".format(os.environ["DB_DIR"])
    ret = json.loads(get_main(cmd, timeout=timeout))
    assert "entries" in ret and "count" in ret and "page" in ret and "num_entries" in ret
    assert len_min <= int(ret["count"]) <= len_max
    if check_func is not None:
        for ent in ret["entries"]:
            assert check_func(ent)
    return ret


def get_error(cmd, error_keyword):
    ret = get_main(cmd)
    assert "ERROR" in ret and error_keyword in ret
    return ret


def get_json_simple(cmd, len_min=0, len_max=sys.maxsize, check_func=None, timeout=DEF_TIMEOUT):
    ret = json.loads(get_main(cmd, timeout=timeout))
    assert "entries" not in ret
    assert len_min <= len(ret) <= len_max
    if check_func is not None:
        for ent in ret:
            assert check_func(ent)
    return ret


def get_rdm(cmd, len_min=0, len_max=sys.maxsize, check_func=None, timeout=DEF_TIMEOUT):
    ret = json.loads(get_main(cmd, timeout=timeout))
    assert len_min <= len(ret.keys()) <= len_max
    if check_func is not None:
        assert check_func(ret)
    return ret


def get_dep_graph(cmd, len_min=0, len_max=sys.maxsize, check_func=None, timeout=DEF_TIMEOUT):
    ret = json.loads(get_main(cmd, timeout=timeout))
    assert "dep_graph" in ret
    assert len_min <= len(ret["dep_graph"]) <= len_max
    if check_func is not None:
        assert check_func(ret)
    return ret


def get_raw(cmd, line_count_min=0, line_count_max=sys.maxsize, check_func=None, sort=False, timeout=DEF_TIMEOUT) -> List[str]:
    cmd = cmd + f" --separator={RAW_SEP} --fmt={{S}}{{L}}"
    ret = get_main(cmd=cmd, sort=sort, timeout=timeout)
    try:
        json.loads(ret)
        assert False
    except json.JSONDecodeError:
        pass
    if ret:
        out = ret.split(os.linesep)
        try:
            count = int(out[0])
            out = out[1:]
            assert line_count_min <= count <= line_count_max
        except ValueError:
            assert line_count_min <= len(out) <= line_count_max
        if check_func is not None:
            for ent in out:
                assert check_func(ent)
        return out
    else:
        assert False


def get_pid(cmd, timeout=DEF_TIMEOUT):
    cmd = cmd + f" --separator={RAW_SEP}"
    ret = get_main(cmd, timeout=timeout)
    try:
        ret = int(ret)
    except ValueError:
        assert False
    return ret


def get_main(cmd, sort=False, timeout=DEF_TIMEOUT):
    cmd = cmd + (" --sorted" if sort else "")

    if in_debug():
        ret = process_commandline(nfsdb, cmd)
    else:
        ret = timeout_decorator.timeout(seconds=timeout, use_signals=True)(process_commandline)(nfsdb, cmd)

    if ret:
        if isinstance(ret, Iterator):
            return os.linesep.join(ret)
        elif isinstance(ret, Generator):
            return os.linesep.join(ret)
        else:
            return ret
    return ""

class Texe(timeout_decorator.TimeoutError):
    pass

def is_command(obj):
    return "class" in obj and obj["class"] in ["linker", "compiler", "linked", "compilation", "command"] and \
        "pid" in obj and isinstance(obj["pid"], int) and \
        "idx" in obj and isinstance(obj["idx"], int) and \
        "ppid" in obj and isinstance(obj["ppid"], int) and \
        "pidx" in obj and isinstance(obj["pidx"], int) and \
        "bin" in obj and isinstance(obj["bin"], str) and \
        "cwd" in obj and isinstance(obj["cwd"], str) and \
        "command" in obj and isinstance(obj["command"], str)


def is_raw_command(obj):
    row = obj.split(RAW_SEP)
    return len(row) == 10 and \
        row[0].isnumeric() and \
        row[1].isnumeric() and \
        row[2].isnumeric() and \
        row[3].split(":")[0].isnumeric() and row[3].split(":")[1].isnumeric() and \
        os.path.normpath(row[4]) and \
        os.path.normpath(row[5])


def is_raw_process(obj):
    row = obj.split(RAW_SEP)
    return len(row) == 2 and \
        row[0].isnumeric() and \
        (row[1] == "r" or row[1] == "w" or row[1] == "rw")


def is_comp_info(obj):
    return "command" in obj and isinstance(obj["command"], dict) and \
        "pid" in obj["command"] and isinstance(obj["command"]["pid"], int) and \
        "idx" in obj["command"] and isinstance(obj["command"]["idx"], int) and \
        "ppid" in obj["command"] and isinstance(obj["command"]["ppid"], int) and \
        "pidx" in obj["command"] and isinstance(obj["command"]["pidx"], int) and \
        "bin" in obj["command"] and isinstance(obj["command"]["bin"], str) and \
        "cwd" in obj["command"] and isinstance(obj["command"]["cwd"], str) and \
        "command" in obj["command"] and isinstance(obj["command"]["command"], str) and \
        "source_type" in obj and isinstance(obj["source_type"], str) and \
        "compiled_files" in obj and isinstance(obj["compiled_files"], list) and \
        isinstance(obj["compiled_files"][0], str) and \
        "include_paths" in obj and isinstance(obj["include_paths"], list) and \
        "headers" in obj and isinstance(obj["headers"], list) and \
        "defs" in obj and isinstance(obj["defs"], list) and \
        "undefs" in obj and isinstance(obj["undefs"], list) and \
        "deps" in obj and isinstance(obj["deps"], list)


def is_raw_comp_info(rows):
    return rows[0] == 'COMPILATION:' and rows[1].startswith('  PID: ')


def is_normpath(obj):
    return isinstance(obj, str) and (os.path.normpath(obj) == obj or obj == '')


def is_linked(obj):
    return obj.split("/")[-1].split(".")[-1] in link_ext


def is_compiled(obj):
    return obj.split("/")[-1].split(".")[-1] in comp_ext


def is_open_details(obj):
    return "filename" in obj and isinstance(obj["filename"], str) and \
        "type" in obj and isinstance(obj["type"], str) and \
        "access" in obj and isinstance(obj["access"], str) and \
        "exists" in obj and isinstance(obj["exists"], int) and \
        "link" in obj and isinstance(obj["link"], int)


def is_raw_open_details(obj):
    row = obj.split(RAW_SEP)
    return len(row) == 7 and \
        os.path.normpath(row[0]) and \
        os.path.normpath(row[1]) and \
        row[2].isnumeric() and \
        (row[3] in ["nonexistent", "file", "dir", "char", "block", "link", "fifo", "sock"]) and \
        (row[4] == "r" or row[4] == "w" or row[4] == "rw") and \
        row[5].isnumeric() and \
        row[6].replace("-", "").isnumeric()


def is_process(obj):
    return "access" in obj and obj["access"] in ["r", "w", "rw"] and \
        "pid" in obj and isinstance(obj["pid"], int)


def is_generated(obj):
    return "directory" in obj and "command" in obj


def is_raw_generated(obj):
    row = obj.split(RAW_SEP)
    return len(row) == 3


def is_raw_generated_openrefs(obj):
    row = obj.split(RAW_SEP)
    return len(row) == 4


def is_generated_openrefs(obj):
    return "directory" in obj and "command" in obj and "openfiles" in obj


def is_makefile(obj):
    return obj[0] == ".PHONY: all"


def is_rdm(obj):
    return isinstance(obj, dict) and isinstance(next(iter(obj)), str) and \
        isinstance(obj[next(iter(obj))], list) and \
        isinstance(obj[next(iter(obj))][0], str)


def is_cdm(obj):
    assert isinstance(obj, dict)
    for link_pth in obj:
        assert is_linked(link_pth)
        for comp_pth in obj[link_pth]:
            assert is_compiled(comp_pth)
    return True


def is_dep_graph(obj):
    return isinstance(obj, dict) and "dep_graph" in obj and \
        isinstance(obj["dep_graph"][next(iter(obj["dep_graph"]))], list) and \
        isinstance(obj["dep_graph"][next(iter(obj["dep_graph"]))][0][0][0], int)


def is_raw_dep_graph(rows):
    return os.path.normpath(rows[0]) and \
        "PIDS:" in rows[1]


def is_raw_rdm(row):
    if row.startswith("  /"):
        return is_linked(row)
    else:
        return is_normpath(row)


def is_raw_cdm(row):
    if row.startswith("/"):
        return is_linked(row)
    elif row.startswith("  /"):
        return is_compiled(row)
    else:
        return False


class TestGeneric:
    @staticmethod
    def trigger_opn_filter_exception(args, assert_message):
        try:
            common_args, pipeline_args, _ = get_args(args)
            parsed_args = merge_args(common_args, pipeline_args[0])
            OpenFilter(parsed_args.open_filter, None, config, nfsdb.source_root)
            assert False
        except FilterException as err:
            if assert_message in err.message:
                assert True
            else:
                assert False

    @staticmethod
    def trigger_cmd_filter_exception(args, assert_message):
        try:
            common_args, pipeline_args, _ = get_args(args)
            parsed_args = merge_args(common_args, pipeline_args[0])
            CommandFilter(parsed_args.command_filter, None, config, nfsdb.source_root)
            assert False
        except FilterException as err:
            if assert_message in err.message:
                assert True
            else:
                assert False

    @staticmethod
    def get_parsed_opn_filter(args):
        try:
            common_args, pipeline_args, _ = get_args(args)
            parsed_args = merge_args(common_args, pipeline_args[0])
            return OpenFilter(parsed_args.open_filter, None, config, nfsdb.source_root)
        except FilterException:
            assert False

    @staticmethod
    def get_parsed_cmd_filter(args):
        try:
            common_args, pipeline_args, _ = get_args(args)
            parsed_args = merge_args(common_args, pipeline_args[0])
            return CommandFilter(parsed_args.command_filter, None, config, nfsdb.source_root)
        except FilterException:
            assert False

    def test_arg_parser(self):
        args = ["deps_for", "--path", "aaa", "--path", "bbb"]
        common_args, pipeline_args, _ = get_args(args)
        parsed_args = merge_args(common_args, pipeline_args[0])
        assert parsed_args.module == get_api_modules().get(args[0].replace("--", ""), None)
        assert len(parsed_args.path) == 2 and parsed_args.path[0] == args[2] and parsed_args.path[1] == args[4]

    def test_argparser(self):
        r = get_bash_complete()
        for c in get_api_keywords():
            assert c in r

    def test_filter_no_module(self):
        self.trigger_opn_filter_exception(["linked_modules", "--filter=[bad_key=bad_value]"], "Unknown filter parameter bad_key")

    def test_filter_bad_key(self):
        self.trigger_opn_filter_exception(["linked_modules", "--filter=[bad_key=bad_value]"], "Unknown filter parameter bad_key")

    def test_filter_dict(self):
        OpenFilter([[{"path": "aa"}, {"class": "compiled"}], [{"path": "bb"}]], None, config, nfsdb.source_root)

    def test_filter_dict_bad(self):
        try:
            OpenFilter([[{"path": "aa"}, {"class": "compiled"}], [{"path": "bb"}], [{"bad_path": "aa"}]], None, config, nfsdb.source_root)
        except FilterException as err:
            if "Unknown filter parameter bad_path" in err.message:
                assert True
            else:
                assert False
        try:
            CommandFilter([[{"bin": "/bin/bash"}, {"class": "compiler"}], [{"bin": "/bin/sh"}], [{"bad_path": "aa"}]], None, config, nfsdb.source_root)
        except FilterException as err:
            if "Unknown filter parameter bad_path" in err.message:
                assert True
            else:
                assert False

    def test_filter_bad_value(self):
        self.trigger_opn_filter_exception(["linked_modules", "--filter=[path=/some_file,type=bad_value]or[path=/some_file,type=re]"], "parameter value")

    def test_filter_missing_primary_key(self):
        self.trigger_opn_filter_exception(["linked_modules", "--filter=[type=re]"], "sub-parameter present but without associated typed parameter")

    def test_filter_multiple_primary_key(self):
        self.trigger_cmd_filter_exception(["linked_modules", "--command-filter=[cwd=/1,bin=/2]"], "More than one typed parameter! Choose between")

    def test_filter_path(self):
        fil = self.get_parsed_opn_filter(["linked_modules", "--filter=[path=/abc,type=wc]"])
        assert len(fil.filter_dict) == 1 and len(fil.filter_dict[0]) == 1
        assert fil.filter_dict[0][0]["path"] == "/abc" and "path_pattern" in fil.filter_dict[0][0]

    def test_filter_bin(self):
        fil = self.get_parsed_cmd_filter(["linked_modules", "--command-filter=[bin=/abc,type=wc]"])
        assert len(fil.filter_dict) == 1 and len(fil.filter_dict[0]) == 1
        assert fil.filter_dict[0][0]["bin"] == "/abc" and "bin_pattern" in fil.filter_dict[0][0]

    def test_filter_cwd(self):
        fil = self.get_parsed_cmd_filter(["linked_modules", "--command-filter=[cwd=/abc,type=wc]"])
        assert len(fil.filter_dict) == 1 and len(fil.filter_dict[0]) == 1
        assert fil.filter_dict[0][0]["cwd"] == "/abc" and "cwd_pattern" in fil.filter_dict[0][0]        

    def test_filter_cmd(self):
        fil = self.get_parsed_cmd_filter(["linked_modules", "--command-filter=[cmd=*sample_command*,type=wc]and[cmd=sample_command,type=sp]and[cmd=.*sample_command.*,type=re]"])
        assert len(fil.filter_dict) == 1 and len(fil.filter_dict[0]) == 3
        assert fil.filter_dict[0][0]["cmd"] == "*sample_command*" and "cmd_pattern" in fil.filter_dict[0][0]
        assert fil.filter_dict[0][1]["cmd"] == "*sample_command*" and "cmd_pattern" in fil.filter_dict[0][1]
        assert fil.filter_dict[0][2]["cmd"] == ".*sample_command.*" and "cmd_pattern" in fil.filter_dict[0][2]

    def test_filter_path_anded(self):
        fil = self.get_parsed_opn_filter(["linked_modules", "--filter=[path=/abc,type=wc]and[path=/def,type=wc]"])
        assert len(fil.filter_dict) == 1 and len(fil.filter_dict[0]) == 2
        assert fil.filter_dict[0][0]["path"] == "/abc" and "path_pattern" in fil.filter_dict[0][0]
        assert fil.filter_dict[0][1]["path"] == "/def" and "path_pattern" in fil.filter_dict[0][1]

    def test_filter_path_ored(self):
        fil = self.get_parsed_opn_filter(["linked_modules", "--filter=[path=/abc,type=wc]and[path=/123,type=wc]or[path=/def,type=wc]and[path=/ghi,type=wc]and[path=/jkl,type=wc]"])
        assert len(fil.filter_dict) == 2 and len(fil.filter_dict[0]) == 2 and len(fil.filter_dict[1]) == 3
        assert fil.filter_dict[0][0]["path"] == "/abc" and "path_pattern" in fil.filter_dict[0][0]
        assert fil.filter_dict[0][1]["path"] == "/123" and "path_pattern" in fil.filter_dict[0][1]
        # OR
        assert fil.filter_dict[1][0]["path"] == "/def" and "path_pattern" in fil.filter_dict[1][0]
        assert fil.filter_dict[1][1]["path"] == "/ghi" and "path_pattern" in fil.filter_dict[1][1]
        assert fil.filter_dict[1][2]["path"] == "/jkl" and "path_pattern" in fil.filter_dict[1][2]

    def test_opn_etrace_filter(self):
        fil = self.get_parsed_opn_filter([
            "linked_modules",
            "--filter=[path=/abc,type=wc,link=1]"
            "and[path=/123,type=re,source_root=1,negate=1]"
            "or[path=/def,type=wc,exists=0,link=1,class=compiled,source_type=other]"
            "and[path=/ghi,type=wc,exists=1,class=linked,access=rw,source_type=c]"
            "and[path=/jkl,type=wc,class=plain,source_root=0,exists=0,access=r,source_type=c++]"])
        
        # (PATH,CLASS,EXISTS,ACCESS,NEGATE,SRCROOT)

        # "linked" : 0x0001,
        # "linked_static" : 0x0002,
        # "linked_shared" : 0x0004,
        # "linked_exe" : 0x0008,
        # "compiled" : 0x0010,
        # "plain" : 0x0020,
        # "compiler" : 0x0040,
        # "linker" : 0x0080,
        # "binary" : 0x0100,
        # "symlink" : 0x0200,
        # "nosymlink" : 0x0400

        assert fil.libetrace_filter[0][0] == (('matches_wc', '/abc'), ('is_class', 0x0200), None, None, False, None, None)
        assert fil.libetrace_filter[0][1] == (('matches_re', '/123'), None, None, None, True, ('at_source_root', None), None)

        assert fil.libetrace_filter[1][0] == (('matches_wc', '/def'), ('is_class', 0x0200 | 0x0010), ('file_not_exists', None), None, False, None,  ('source_type', 4))
        assert fil.libetrace_filter[1][1] == (('matches_wc', '/ghi'), ('is_class', 0x0001), ('file_exists', None), ('has_access', 2), False, None, ('source_type', 1))
        assert fil.libetrace_filter[1][2] == (('matches_wc', '/jkl'), ('is_class', 0x0020), ('file_not_exists', None), ('has_access', 0), False, ('not_at_source_root', None), ('source_type', 2))

    def test_cmd_etrace_filter(self):
        fil = self.get_parsed_cmd_filter([
            "linked_modules",
            "--command-filter=[bin=/abc,type=wc,class=linker]"
            "and[cwd=/123,type=re,cwd_source_root=1,negate=1,class=command]"
            "or[cmd=comm,type=wc,class=compiler]"
            "and[bin=/ghi,type=wc,class=linker,negate=1]"
            "and[ppid=123,class=command,bin_source_root=0]"
            "and[bin=/bash]"])

        # (STR,PPID,CLASS,NEGATE,SRCROOT)

        # "command" : 0x0001,
        # "compiler" : 0x0002,
        # "linker" : 0x0004,

        assert fil.libetrace_filter[0][0] == (('bin_matches_wc', '/abc'), None, ('is_class', 0x0004), False, None)
        assert fil.libetrace_filter[0][1] == (('cwd_matches_re', '/123'), None, ('is_class', 0x0001), True, ('cwd_at_source_root', None))

        assert fil.libetrace_filter[1][0] == (('cmd_matches_wc', 'comm'), None, ('is_class', 0x0002), False, None)
        assert fil.libetrace_filter[1][1] == (('bin_matches_wc', '/ghi'), None, ('is_class', 0x0004), True, None)
        assert fil.libetrace_filter[1][2] == (None, ("has_ppid", 123), ('is_class', 0x0001), False, ('bin_not_at_source_root', None))
        assert fil.libetrace_filter[1][3] == (('bin_contains_path', '/bash'), None, None, False, None)


class TestBinaries:
    def test_simple(self):
        get_json_entries("binaries -n=0 --json", 1000, 3000, is_normpath)

    def test_simple_raw(self):
        get_raw("binaries -n=0", 1000, 3000, is_normpath)

    def test_simple_relative(self):
        ret = get_json_entries("binaries -n=0 --json --relative", 1000, 3000, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_simple_relative_raw(self):
        ret = get_raw("binaries -n=0 --relative", 1000, 3000, is_normpath)
        for ent in ret:
            assert not ent.startswith(nfsdb.source_root)

    def test_with_bin(self):
        ret = get_json_entries("binaries --command-filter=[bin=*/bin/bash,type=wc]or[bin=*/bin/sh,type=wc] --json", 2, 2, is_normpath)
        for ent in ret["entries"]:
            assert fnmatch.fnmatch(ent, "*/bin/*sh")

    def test_with_bin_raw(self):
        ret = get_raw("binaries --command-filter=[bin=*/bin/bash,type=wc]or[bin=*/bin/sh,type=wc]", 2, 2, is_normpath)
        for ent in ret:
            assert fnmatch.fnmatch(ent, "*/bin/*sh")

    def test_with_path_details(self, javac):
        ret = get_json_entries(f"binaries -n=100 --command-filter=[bin={javac}] --details --json", 600, 1500, is_command)
        for ent in ret["entries"]:
            assert ent['bin'] == javac

    def test_with_path_details_raw(self, javac):
        ret = get_raw(f"binaries -n=100 --command-filter=[bin={javac}] --details", 600, 1500, is_raw_command)
        for ent in ret:
            assert javac in ent

    def test_with_filter(self):
        ret = get_json_entries("binaries -n=10 --command-filter=[bin=*.py,type=wc] --json", 50, 150, is_normpath)
        for ent in ret["entries"]:
            assert fnmatch.fnmatch(ent, "*.py")

    def test_with_filter_raw(self):
        ret = get_raw("binaries -n=10 --command-filter=[bin=*.py,type=wc]", 50, 150, is_normpath)
        for ent in ret:
            assert fnmatch.fnmatch(ent, "*.py")

    def test_compiler(self):
        ret = get_json_entries("binaries -n=0 --command-filter=[class=compiler] --json", 4, 20, is_normpath)
        comp_wc = config.config_info["gcc_spec"] + config.config_info["gpp_spec"] + config.config_info["clang_spec"] + config.config_info["clangpp_spec"] + config.config_info["armcc_spec"]
        for ent in ret["entries"]:
            assert any([len(fnmatch.filter([ent], wc)) > 0 for wc in comp_wc])

    def test_compiler_raw(self):
        ret = get_raw("binaries --command-filter=[class=compiler]", 4, 20, is_normpath)
        comp_wc = config.config_info["gcc_spec"] + config.config_info["gpp_spec"] + config.config_info["clang_spec"] + config.config_info["clangpp_spec"] + config.config_info["armcc_spec"]
        for ent in ret:
            assert any([len(fnmatch.filter([ent], wc)) > 0 for wc in comp_wc])

    def test_linker(self):
        ret = get_json_entries("binaries -n=0 --command-filter=[class=linker] --json", 4, 20, is_normpath)
        link_wc = config.config_info["ld_spec"] + config.config_info["ar_spec"] + ["/bin/bash"]
        for ent in ret["entries"]:
            assert any([len(fnmatch.filter([ent], wc)) > 0 for wc in link_wc])

    def test_linker_raw(self):
        ret = get_raw("binaries -n=0 --command-filter=[class=linker]", 4, 20, is_normpath)
        link_wc = config.config_info["ld_spec"] + config.config_info["ar_spec"] + ["/bin/bash"]
        for ent in ret:
            assert any([len(fnmatch.filter([ent], wc)) > 0 for wc in link_wc])

    def test_details(self):
        ret = get_json_entries("binaries -n=100 --command-filter=[class=compiler] --details --json", 50000, 100000, is_command)
        for ent in ret["entries"]:
            assert ent["class"] == "compiler"

    def test_details_raw(self):
        ret = get_raw("binaries -n=100 --command-filter=[class=compiler] --details", 50000, 100000, is_raw_command)
        for ent in ret:
            assert ent.split(RAW_SEP)[8].split(":")[1].startswith("c")

    def test_src_root(self):
        total_count = get_json_simple("binaries -n=1 --json --details -l")["count"]
        ret = get_json_entries("binaries -n=1000 --command-filter=[bin_source_root=1] --json --details", 50000, 900000, is_command)
        for ent in ret["entries"]:
            assert ent["bin"].startswith(nfsdb.source_root)
        sr_count = int(ret['count'])
        ret = get_json_entries("binaries -n=1000 --command-filter=[bin_source_root=0] --json --details", 50000, 900000, check_func=is_command)
        for ent in ret["entries"]:
            assert not ent["bin"].startswith(nfsdb.source_root)
        nsr_count = int(ret['count'])
        assert total_count == (sr_count + nsr_count)

    def test_src_root_raw(self):
        ret = get_raw("binaries --command-filter=[bin_source_root=1] --details -n=1000", 50000, 900000, check_func=is_raw_command)
        for ent in ret:
            assert ent.split(RAW_SEP)[4].startswith(nfsdb.source_root)

        ret = get_raw("binaries --command-filter=[bin_source_root=0] --details -n=1000", 50000, 900000, check_func=is_raw_command)
        for ent in ret:
            assert not ent.split(RAW_SEP)[4].startswith(nfsdb.source_root)

    def test_compile_commands(self):
        ret = get_json_entries("binaries --command-filter=[bin=*clang,type=wc,class=compiler] --commands --json", 15000, 150000, check_func=is_command)
        for ent in ret["entries"]:
            assert ent["class"] == "compiler"
            assert ent["bin"].endswith("clang")

    def test_compile_commands_raw(self):
        ret = get_raw("binaries --command-filter=[bin=*clang,type=wc,class=compiler] --commands", 15000, 150000, check_func=is_raw_command)
        for ent in ret:
            assert ent.split(RAW_SEP)[4].endswith("clang")

    def test_linking_commands(self):
        ret = get_json_entries("binaries --command-filter=[bin=*ld.lld,type=wc,class=linker] --commands --json", 4000, 15000, check_func=is_command)
        for ent in ret["entries"]:
            assert ent["class"] == "linker"
            assert ent["bin"].endswith("ld.lld")

    def test_linking_commands_raw(self):
        ret = get_raw("binaries --command-filter=[bin=*ld.lld,type=wc,class=linker] --commands", 4000, 15000, check_func=is_raw_command)
        for ent in ret:
            assert ent.split(RAW_SEP)[4].endswith("ld.lld")

    def test_count(self):
        assert get_json_entries("binaries --json ")["count"] == get_json_simple("binaries --json -l")["count"]


class TestCommands:
    def test_plain(self):
        get_json_entries("commands --json", 500000, 2500000, is_command)

    def test_plain_raw(self):
        get_raw("commands -n=1000", 500000, 2500000, check_func=is_raw_command)

    def test_with_filter(self):
        ret = get_json_entries("commands --command-filter=[bin=*prebuilts*javac*,type=wc] --json", 1000, 4000, check_func=is_command)
        for ent in ret["entries"]:
            assert ("javac" in ent["bin"] and "prebuilts" in ent["bin"])

    def test_with_filter_raw(self):
        ret = get_raw("commands --command-filter=[bin=*prebuilts*javac*,type=wc]", 1000, 4000, check_func=is_raw_command)
        for ent in ret:
            assert ("javac" in ent and "prebuilts" in ent)

    def test_with_pid(self, pids):
        ret = get_json_entries(f"commands --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --json", 3, 50, check_func=is_command)
        for ent in ret["entries"]:
            assert ent['pid'] in pids

    def test_with_pid_raw(self, pids):
        ret = get_raw(f"commands --pid={pids[0]} --pid={pids[1]} --pid={pids[2]}", 3, 50, check_func=is_raw_command)
        for ent in ret:
            assert int(ent.split(RAW_SEP)[0]) in pids

    def test_details(self):
        ret = get_json_entries("commands --command-filter=[bin=*prebuilts*javac*,type=wc] --details --json", 1000, 4000, check_func=is_command)
        for ent in ret["entries"]:
            assert ("javac" in ent["bin"] and "prebuilts" in ent["bin"])

    def test_details_raw(self):
        ret = get_raw("commands --command-filter=[bin=*prebuilts*javac*,type=wc] --details", 1000, 4000, check_func=is_raw_command)
        for ent in ret:
            assert ("javac" in ent and "prebuilts" in ent)

    def test_generate(self):
        ret = get_json_simple("commands --command-filter=[bin=*clang*,type=wc] --generate --json", 1000, 1000, check_func=is_generated)
        for ent in ret:
            assert"clang" in ent["command"]

    def test_generate_raw(self):
        ret = get_raw("commands --command-filter=[bin=*clang*,type=wc] --generate", 1000, 200000, check_func=is_raw_generated)
        for ent in ret:
            assert "clang" in ent.split(RAW_SEP)[0]

    def test_generate_makefile(self):
        ret = get_raw("commands --command-filter=[bin=*/bin/clang,type=wc] --generate -n=0 --makefile")
        assert is_makefile(ret)

    def test_generate_makefile_all(self):
        ret = get_raw("commands --command-filter=[bin=*java,type=wc] --generate --makefile --all")
        assert is_makefile(ret)

    def test_class_linker(self):
        ret = get_json_entries("commands --command-filter=[class=linker] --json", 5000, 15000, check_func=is_command)
        link_wc = config.config_info["ld_spec"] + config.config_info["ar_spec"] + ["/bin/bash"]
        for ent in ret['entries']:
            matched = [len(fnmatch.filter([ent['bin'].split()[0]], wc)) > 0 for wc in link_wc]
            if not any(matched):
                assert False

    def test_class_linker_raw(self):
        ret = get_raw("commands --command-filter=[class=linker]", 5000, 15000, check_func=is_raw_command)
        link_wc = config.config_info["ld_spec"] + config.config_info["ar_spec"] + ["/bin/bash"]
        for ent in ret:
            matched = [len(fnmatch.filter([ent.split(RAW_SEP)[4]], wc)) > 0 for wc in link_wc]
            if not any(matched):
                assert False

    def test_class_compiler(self):
        ret = get_json_entries("commands --command-filter=[class=compiler] --json", 50000, 150000, check_func=is_command)
        comp_wc = config.config_info["gcc_spec"] + config.config_info["gpp_spec"] + config.config_info["clang_spec"] + config.config_info["clangpp_spec"] + config.config_info["armcc_spec"]
        for ent in ret['entries']:
            matched = [len(fnmatch.filter([ent['bin'].split()[0]], wc)) > 0 for wc in comp_wc]
            if not any(matched):
                assert False

    def test_class_compiler_raw(self):
        ret = get_raw("commands --command-filter=[class=compiler]", 50000, 150000, check_func=is_raw_command)
        comp_wc = config.config_info["gcc_spec"] + config.config_info["gpp_spec"] + config.config_info["clang_spec"] + config.config_info["clangpp_spec"] + config.config_info["armcc_spec"]
        for ent in ret:
            matched = [len(fnmatch.filter([ent.split(RAW_SEP)[4]], wc)) > 0 for wc in comp_wc]
            if not any(matched):
                assert False

    def test_path_plain(self, sh, bash):
        ret = get_json_entries(f"commands --binary={sh} --binary={bash} --json", 20000, 500000, check_func=is_command)
        for ent in ret["entries"]:
            assert ent["bin"] in ["/usr/bin/sh", "/usr/bin/bash"]

    def test_path_plain_raw(self, sh, bash):
        ret = get_raw(f"commands --binary={sh} --binary={bash}", 20000, 500000, check_func=is_raw_command, sort=False)
        for ent in ret:
            assert ent.split(RAW_SEP)[4] == sh or ent.split(RAW_SEP)[4] == bash

    def test_path_filter_cmd(self, sh, bash):
        ret = get_json_entries(f"commands --binary={sh} --binary={bash} --command-filter=[cmd=.*vmlinux.*,type=re] --json", 10, 100, check_func=is_command)
        for ent in ret["entries"]:
            assert (ent["bin"] == bash or ent["bin"] == sh) or "vmlinux" in ent["command"]

    def test_path_filter_cmd_raw(self, sh, bash):
        ret = get_raw(f"commands --binary={sh} --binary={bash} --command-filter=[cmd=.*vmlinux.*,type=re]", 10, 100, is_raw_command, sort=False)
        for ent in ret:
            assert (ent.split(RAW_SEP)[4] == bash or ent.split(RAW_SEP)[4] == sh) and "vmlinux" in ent.split(RAW_SEP)[6]

    def test_path_filter_cwd(self, javac):
        ret = get_json_entries(f"commands --binary={javac} --command-filter=[cwd=*android*,type=wc] --json", 300, 1500, is_command)
        for ent in ret["entries"]:
            assert ent["bin"] == javac and "android" in ent["cwd"]

    def test_path_filter_cwd_raw(self, javac):
        ret = get_raw(f"commands --binary={javac} --command-filter=[cwd=*android*,type=wc]", 300, 1500, is_raw_command)
        for ent in ret:
            assert ent.split(RAW_SEP)[4] == javac and "android" in ent.split(RAW_SEP)[5]

    def test_path_filter_class(self, linker):
        ret = get_json_entries(f"commands --binary={linker} --command-filter=[class=linker] --json", 50, 6000, is_command)
        for ent in ret["entries"]:
            assert ent['linked'] is not None

    def test_path_filter_class_raw(self, linker):
        ret = get_raw(f"commands --binary={linker} --command-filter=[class=linker]", 50, 6000, is_raw_command)
        for ent in ret:
            assert ent.split(RAW_SEP)[4] == linker

    def test_count(self):
        assert get_json_entries("commands --json -n=1")["count"] == get_json_simple("commands --json -l")["count"]


class TestCompilationInfo:
    def test_required_param(self):
        get_error("compilation_info_for", error_keyword="Missing required args 'path'")

    def test_plain(self, compiled_file):
        ret = get_json_entries(f"compilation_info_for --path={compiled_file} --json", 1, 100, is_comp_info)
        for ent in ret["entries"]:
            assert compiled_file in ent["compiled_files"]

    def test_plain_raw(self, compiled_file):
        ret = get_raw(f"compilation_info_for --path={compiled_file}")
        assert is_raw_comp_info(ret)
        assert "    {}".format(compiled_file) in ret

    def test_plain_command_filter(self, compiled_file):
        ret = get_json_entries(f"compilation_info_for --command-filter=[bin=*clang,type=wc] --path={compiled_file} --json", 1, 100, is_comp_info)
        for ent in ret["entries"]:
            assert compiled_file in ent["compiled_files"] and ent['command']['bin'].endswith('clang')

        get_json_entries(f"compilation_info_for --command-filter=[bin=*dummy_exec,type=wc] --path={compiled_file} --json", 0, 0, is_comp_info)

    def test_pipeline(self):
        ret = get_json_entries("compiled --filter=[path=*curl/lib*,type=wc] compilation_info_for -n=10 --json", 400, 1500, is_comp_info)
        for ent in ret["entries"]:
            for fil in ent["compiled_files"]:
                assert is_compiled(fil)

    def test_pipeline_raw(self):
        ret = get_raw("compiled --filter=[path=*curl/lib*,type=wc] compilation_info_for")
        assert is_raw_comp_info(ret)

    def test_count(self):
        assert get_json_entries("compiled --filter=[path=*curl/lib*,type=wc] compilation_info_for --json ")["count"] == \
            get_json_simple("compiled --filter=[path=*curl/lib*,type=wc] compilation_info_for --json -l")["count"]


class TestCompiled:
    def test_plain(self):
        ret = get_json_entries("compiled -n=0 --json", 20000, 80000, is_normpath)
        for ent in ret["entries"]:
            assert is_compiled(ent)

    def test_plain_raw(self):
        ret = get_raw("compiled", 20000, 80000, is_normpath)
        for ent in ret:
            assert is_compiled(ent)

    def test_plain_relative(self):
        ret = get_json_entries("compiled -n=0 --json --relative", 20000, 80000, is_normpath)
        for ent in ret["entries"]:
            assert is_compiled(ent) and not ent.startswith(nfsdb.source_root)

    def test_plain_relative_raw(self):
        ret = get_raw("compiled --relative", 20000, 80000, is_normpath)
        for ent in ret:
            assert is_compiled(ent) and not ent.startswith(nfsdb.source_root)

    def test_with_filter(self):
        ret = get_json_entries("compiled --filter=[path=*.c,type=wc,exists=1,access=r] --json", 8000, 25000, is_normpath)
        for ent in ret["entries"]:
            assert ent.endswith(".c")

    def test_with_filter_raw(self):
        ret = get_raw("compiled --filter=[path=*.c,type=wc,exists=1,access=r]", 8000, 25000, is_normpath)
        for ent in ret:
            assert ent.endswith(".c")

    def test_details(self):
        ret = get_json_entries("compiled --filter=[path=*.cpp,type=wc] --details --json", 20000, 50000, is_open_details)
        for ent in ret["entries"]:
            assert ent['filename'].endswith(".cpp")

    def test_details_raw(self):
        ret = get_raw("compiled --filter=[path=*.cpp,type=wc] --details", 20000, 50000, is_raw_open_details)
        for ent in ret:
            assert ent.split(RAW_SEP)[0].endswith(".cpp")

    def test_details_relative(self):
        ret = get_json_entries("compiled --filter=[path=*.cpp,type=wc] --details --json --relative", 20000, 50000, is_open_details)
        for ent in ret["entries"]:
            assert ent['filename'].endswith(".cpp") and not ent['filename'].startswith(nfsdb.source_root)

    def test_details_relative_raw(self):
        ret = get_raw("compiled --filter=[path=*.cpp,type=wc] --details --relative", 20000, 50000, is_raw_open_details)
        for ent in ret:
            assert ent.split(RAW_SEP)[0].endswith(".cpp") and not ent.split(RAW_SEP)[0].startswith(nfsdb.source_root)

    def test_commands(self):
        ret = get_json_entries("compiled --filter=[path=*.cpp,type=wc] --commands --command-filter=[bin=*clang++,type=wc] --json", 20000, 50000, is_command)
        for ent in ret["entries"]:
            assert any([c for c,t in ent['compiled'].items() if c.endswith(".cpp")]) and ent['bin'].endswith("/clang++")

    def test_commands_raw(self):
        ret = get_raw("compiled --filter=[path=*.cpp,type=wc] --commands --command-filter=[bin=*clang++,type=wc] ", 20000, 50000, is_raw_command)
        for ent in ret:
            assert ent.split(RAW_SEP)[8].split(":")[0].endswith(".cpp") and \
            ent.split(RAW_SEP)[4].endswith("/clang++")

    def test_compile_commands(self):
        ret = get_json_entries("compiled --commands --json -n=0", 50000, 150000, is_command, timeout=10)
        for ent in ret["entries"]:
            assert ent["class"] == "compiler"

    def test_compile_commands_raw(self):
        ret = get_raw("compiled --commands", 50000, 150000, is_raw_command, timeout=10)
        for ent in ret:
            assert ent.split(RAW_SEP)[8].split(":")[1].startswith("c")

    def test_relative(self):
        ret = get_json_entries("compiled --json --relative", 10000, 50000, is_normpath)
        s_r = get_main("source_root")
        for ent in ret["entries"]:
            assert not ent.startswith(s_r)

    def test_relative_raw(self):
        ret = get_raw("compiled --relative", 10000, 50000, is_normpath)
        s_r = get_main("source_root")
        for ent in ret:
            assert not ent.startswith(s_r)

    def test_count(self):
        assert get_json_entries("compiled --json ")["count"] == \
            get_json_simple("compiled --json -l")["count"]


class TestDepsFor:
    def test_required_param(self):
        get_error("deps_for", error_keyword="Missing required args 'path'")

    def test_plain(self, vmlinux):
        entr = get_json_entries(f"deps_for --path={vmlinux} --json -n=0", 15000, 40000, is_normpath)
        for e in entr['entries']:
            if vmlinux == e:
                assert False, "Requested file in return set - this may be problem with excluded commands in bas_config"

    def test_plain_raw(self, vmlinux):
        entr = get_raw(f"deps_for --path={vmlinux}", 15000, 40000, is_normpath)
        for e in entr:
            if vmlinux == e:
                assert False, "Requested file in return set - this may be problem with excluded commands in bas_config"

    def test_plain_compare(self, vmlinux):
        entr_j = get_json_entries(f"deps_for --path={vmlinux} --json --sorted -n=0", 15000, 40000, is_normpath)
        entr_r = get_raw(f"deps_for --path={vmlinux} --sorted -n=0", 15000, 40000, is_normpath)
        assert len(entr_r) == len(entr_j['entries'])
        for i, e in enumerate(entr_r):
            assert e == entr_j['entries'][i]

    def test_details(self, vmlinux):
        get_json_entries(f"deps_for --path={vmlinux} --json --details", 15000, 40000, is_open_details)

    def test_details_raw(self, vmlinux):
        get_raw(f"deps_for --path={vmlinux} --details", 15000, 40000, is_raw_open_details)

    def test_details_relative(self, vmlinux):
        ret = get_json_entries(f"deps_for --path={vmlinux} --json --details --relative", 15000, 40000, is_open_details)
        for ent in ret["entries"]:
            assert not ent["filename"].startswith(nfsdb.source_root)

    def test_details_relative_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --details --relative", 15000, 40000, is_raw_open_details)
        for ent in ret:
            assert not ent.split(RAW_SEP)[0].startswith(nfsdb.source_root)

    def test_plain_cached(self, vmlinux):
        get_json_entries(f"deps_for --path={vmlinux} --cached --json", 15000, 40000, is_normpath)

    def test_plain_cached_raw(self, vmlinux):
        get_raw(f"deps_for --path={vmlinux} --cached", 15000, 40000, is_normpath)

    def test_relative(self, vmlinux):
        ret = get_json_entries(f"deps_for --path={vmlinux} --json --relative", 15000, 40000, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_relative_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --relative", 15000, 40000, is_normpath)
        for ent in ret:
            assert not ent.startswith(nfsdb.source_root)

    def test_plain_epath(self, vmlinux):
        get_json_entries(f"deps_for --path=[file={vmlinux},direct=true] --json", 1000, 20000, is_normpath)

    def test_plain_epath_raw(self, vmlinux):
        get_raw(f"deps_for --path=[file={vmlinux},direct=true]", 1000, 20000, is_normpath)

    def test_plain_multi(self, vmlinux, vmlinux_o):
        get_json_entries(f"deps_for --path={vmlinux} --path={vmlinux_o} --json", 15000, 40000, is_normpath)

    def test_plain_multi_raw(self, vmlinux, vmlinux_o):
        get_raw(f"deps_for --path={vmlinux} --path={vmlinux_o}", 15000, 40000, is_normpath)

    def test_filters(self, vmlinux):
        ret = get_json_entries(f"deps_for --path={vmlinux} --filter=[path=*.c,type=wc,access=w] --json", 5, 50, is_normpath)
        for ent in ret["entries"]:
            assert ent.endswith(".c")

    def test_filters_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --filter=[path=*.c,type=wc,access=w]", 5, 50, is_normpath)
        for ent in ret:
            assert ent.split(RAW_SEP)[0].endswith(".c")

    def test_class_linked(self, vmlinux):
        ret = get_json_entries(f"deps_for --path={vmlinux} --filter=[class=linked] --json -n=0", 100, 3000, is_normpath)
        for ent in ret["entries"]:
            assert is_linked(ent)

    def test_class_linked_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --filter=[class=linked]", 100, 3000, is_normpath)
        for ent in ret:
            assert is_linked(ent)

    def test_class_compiled(self, vmlinux):
        ret = get_json_entries(f"deps_for --path={vmlinux} --filter=[class=compiled] --json -n=0", 1000, 4000, is_normpath)
        for ent in ret["entries"]:
            assert is_compiled(ent)

    def test_class_compiled_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --filter=[class=compiled]", 1000, 4000, is_normpath)
        for ent in ret:
            assert is_compiled(ent)

    def test_direct(self, vmlinux):
        get_json_entries(f"deps_for --path={vmlinux} --direct --json", 1000, 20000, is_normpath)

    def test_direct_raw(self, vmlinux):
        get_raw(f"deps_for --path={vmlinux} --direct", 1000, 20000, is_normpath)

    def test_commands(self, vmlinux):
        get_json_entries(f"deps_for --path={vmlinux} --commands --json", 2000, 20000, is_command)

    def test_commands_raw(self, vmlinux):
        get_raw(f"deps_for --path={vmlinux} --commands", 2000, 20000, is_raw_command)

    def test_commands_filter(self, vmlinux):
        ret = get_json_entries(f"deps_for --path={vmlinux} --filter=[path=*common.o,type=wc] --commands --command-filter=[bin=*/ld.lld,type=wc] --json", 1, 100, is_command)
        for ent in ret["entries"]:
            assert ent['bin'].endswith("/ld.lld") and ( ent['linked'].endswith("common.o") or ent['linked'].endswith("vmlinux") )

    def test_commands_filter_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --filter=[path=*common.o,type=wc] --commands --command-filter=[bin=*/ld.lld,type=wc] ", 1, 100, is_raw_command)
        for ent in ret:
            assert ent.split(RAW_SEP)[4].endswith("/ld.lld") and ( ent.split(RAW_SEP)[7].endswith("common.o") or ent.split(RAW_SEP)[7].endswith("vmlinux") )

    def test_generate(self, vmlinux):
        get_json_simple(f"deps_for --path={vmlinux} --commands --generate -n=0 --json", 1500, 4000, is_generated)

    def test_generate_raw(self, vmlinux):
        get_raw(f"deps_for --path={vmlinux} --commands --generate", 1500, 4000, is_raw_generated)

    def test_generate_makefile(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --commands --generate --makefile -n=0 --json")
        assert is_makefile(ret)

    def test_generate_makefile_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --commands --generate --makefile")
        assert is_makefile(ret)

    def test_generate_openrefs(self, vmlinux):
        get_json_simple(f"deps_for --path={vmlinux} --commands --generate --openrefs -n=0 --json", 500, 10000, is_generated_openrefs)

    def test_generate_openrefs_raw(self, vmlinux):
        get_raw(f"deps_for --path={vmlinux} --commands --generate --openrefs", 500, 10000, is_raw_generated_openrefs)

    def test_rdm(self, vmlinux):
        ret = get_rdm(f"deps_for --path={vmlinux} --rdm --filter=[path=.*\\.c$|.*\\.h$,type=re] -n=0 --json", 2000, 10000, is_rdm)
        for ent in ret.keys():
            assert ent.endswith(".c") or ent.endswith(".h")
            for fil in ret[ent]:
                assert is_normpath(fil)

    def test_rdm_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --rdm --filter=[path=.*\\.c$|.*\\.h$,type=re]", check_func=is_raw_rdm)
        for ent in ret[:1000]:
            assert ent.startswith("/") or ent.startswith("  /")
            if ent.startswith("/"):
                assert is_normpath(ent)
                assert ent.endswith(".c") or ent.endswith(".h")
            if ent.startswith("  /"):
                assert is_normpath(ent)

    def test_cdm(self, vmlinux):
        get_rdm(f"deps_for --path={vmlinux} --filter=[class=linked] --cdm -n=0 --json", 100, 30000, is_cdm, timeout=5)

    def test_cdm_raw(self, vmlinux):
        get_raw(f"deps_for --path={vmlinux} --filter=[class=linked] --cdm -n=0", check_func=is_raw_cdm, timeout=5)

    def test_revdeps(self, vmlinux):
        get_json_entries(f"deps_for --path={vmlinux} --revdeps --filter=[path=.*\\.c|.*\\.h,type=re] -n=0 --json", 2000, 10000, is_normpath)

    def test_dep_graph(self, vmlinux):
        get_dep_graph(f"deps_for --path={vmlinux} --dep-graph -n=0 --json", 2000, 15000, is_dep_graph, timeout=5)

    def test_dep_graph_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --dep-graph -n=0", timeout=5)
        assert is_raw_dep_graph(ret)

    def test_count(self, vmlinux):
        assert get_json_entries(f"deps_for --path={vmlinux} --json ")["count"] == \
            get_json_simple(f"deps_for --path={vmlinux} --json -l")["count"]


class TestFaccess:
    def test_plain(self, compiled_file):
        get_json_entries(f"faccess --path={compiled_file} --json", 1, 20, is_process)

    def test_plain_raw(self, compiled_file):
        get_raw(f"faccess --path={compiled_file}", 1, 20, is_raw_process)

    def test_details(self, compiled_file):
        ret = get_json_entries(f"faccess --path={compiled_file} --details --json", 1, 20, is_open_details)
        for ent in ret["entries"]:
            assert ent["filename"] == compiled_file

    def test_details_raw(self, compiled_file):
        ret = get_raw(f"faccess --path={compiled_file} --details", 1, 20, is_raw_open_details)
        for ent in ret:
            assert ent.split(RAW_SEP)[0] == compiled_file

    def test_commands(self, compiled_file):
        get_json_entries(f"faccess --path={compiled_file}  --commands --json", 1, 20, is_command)

    def test_commands_raw(self, compiled_file):
        get_raw(f"faccess --path={compiled_file} --commands", 1, 20, is_raw_command)

    def test_generate(self, compiled_file):
        get_json_simple(f"faccess --path={compiled_file} --filter=[access=r] --commands --generate --json", 1, 50000, is_generated)

    def test_generate_raw(self, compiled_file):
        get_raw(f"faccess --path={compiled_file} --filter=[access=r] --commands --generate", 1, 50000, is_raw_generated)

    def test_mode(self, compiled_file):
        ret = get_json_entries(f"faccess --path={compiled_file} --filter=[access=r] --json", 1, 20)
        for ent in ret["entries"]:
            assert ent["access"] == "r"

    def test_mode_raw(self, compiled_file):
        ret = get_raw(f"faccess --path={compiled_file} --filter=[access=r]", 1, 20, is_raw_process)
        for ent in ret:
            assert ent.split(RAW_SEP)[1] == "r"

    def test_count(self, compiled_file):
        assert get_json_entries(f"faccess --path={compiled_file} --json ")["count"] == \
            get_json_simple(f"faccess --path={compiled_file} --json -l")["count"]


class TestLinkedModules:
    def test_plain(self):
        get_json_entries("linked_modules --json", 2000, 15000, is_normpath)

    def test_plain_raw(self):
        get_raw("linked_modules -n=0 ", 2000, 15000, is_normpath)

    def test_filter(self):
        ret = get_json_entries("linked_modules --filter=[path=*vmlinux,type=wc,exists=1]and[compressed,negate=1] --json", 1, 1, is_normpath)
        for ent in ret["entries"]:
            assert ent.endswith("vmlinux")

    def test_filter_raw(self):
        ret = get_raw("linked_modules --filter=[path=*vmlinux,type=wc,exists=1]and[compressed,negate=1]", 1, 1, is_normpath)
        for ent in ret:
            assert ent.endswith("vmlinux")

    def test_commands(self):
        get_json_entries("linked_modules --filter=[path=*vmlinux,type=wc,exists=1]and[compressed,negate=1] --commands --json", 1, 1, is_command)

    def test_commands_raw(self):
        get_raw("linked_modules --filter=[path=*vmlinux,type=wc,exists=1]and[compressed,negate=1] --commands", 1, 1, is_raw_command)

    def test_commands_filter(self):
        ret = get_json_entries("linked_modules --filter=[path=*data.*,type=wc] --commands --command-filter=[bin=*llvm-ar,type=wc] --json", 1, 100, is_command)
        for ent in ret["entries"]:
            assert ent['bin'].endswith("llvm-ar") and ent['linked'].endswith("data.a")

    def test_commands_filter_raw(self):
        ret = get_raw("linked_modules --filter=[path=*data.*,type=wc] --commands --command-filter=[bin=*llvm-ar,type=wc]", 1, 100, is_raw_command)
        for ent in ret:
            assert ent.split(RAW_SEP)[4].endswith("llvm-ar") and ent.split(RAW_SEP)[7].endswith("data.a")

    def test_details(self):
        get_json_entries("linked_modules --details -n=0 --json", 2000, 15000, is_open_details)

    def test_details_raw(self):
        get_raw("linked_modules --details", 2000, 15000, is_raw_open_details)

    def test_linked(self):
        get_json_entries("linked_modules --filter=[class=linked] -n=0 --json", 2000, 15000, is_normpath)

    def test_linked_raw(self):
        get_raw("linked_modules --filter=[class=linked]", 2000, 15000, is_normpath)

    def test_linked_static(self):
        ret = get_json_entries("linked_modules --filter=[class=linked_static] -n=0 --json", 2000, 15000, is_normpath)
        for ent in ret["entries"]:
            assert ent.endswith(".a")

    def test_linked_static_raw(self):
        ret = get_raw("linked_modules --filter=[class=linked_static]", 2000, 15000, is_normpath)
        for ent in ret:
            assert ent.endswith(".a")

    def test_linked_shared(self):
        ret = get_json_entries("linked_modules --filter=[class=linked_shared] -n=0 --json", 2000, 15000, is_normpath)
        for ent in ret["entries"]:
            assert ent.endswith(".so") or ent.endswith(".so.dbg") or ent.endswith(".so.raw") or \
                   ent.endswith(".btf") or ent.endswith("/linker") or ent.endswith("/linker64") or \
                   "tmp.XXX" in ent or "kallsyms" in ent or "vmlinux" in ent or "/tmp/tmp." in ent

    def test_linked_shared_raw(self):
        ret = get_raw("linked_modules --filter=[class=linked_shared]", 2000, 15000, is_normpath)
        for ent in ret:
            assert ent.endswith(".so") or ent.endswith(".so.dbg") or ent.endswith(".so.raw") or \
                   ent.endswith(".btf") or ent.endswith("/linker") or ent.endswith("/linker64") or \
                   "tmp.XXX" in ent or "kallsyms" in ent or "vmlinux" in ent or "/tmp/tmp." in ent

    def test_linked_exe(self):
        ret = get_json_entries("linked_modules --filter=[class=linked_exe] -n=0 --json", 500, 3000, is_normpath)
        for ent in ret["entries"]:
            assert "out/" in ent or "kernel/common" in ent

    def test_linked_exe_raw(self):
        ret = get_raw("linked_modules --filter=[class=linked_exe]", 500, 3000, is_normpath)
        for ent in ret:
            assert "out/" in ent or "kernel/common" in ent

    def test_generate(self):
        get_json_simple("linked_modules --commands --generate --all -n=0 --json", 2000, 15000, is_generated)

    def test_generate_raw(self):
        get_raw("linked_modules --commands --generate --all", 2000, 15000, is_raw_generated)

    def test_relative(self):
        ret = get_json_entries("linked_modules --json --relative", 2000, 15000, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_relative_raw(self):
        ret = get_raw("linked_modules --relative", 2000, 15000, is_normpath)
        for ent in ret:
            assert not ent.startswith(nfsdb.source_root)

    def test_details_relative(self):
        ret = get_json_entries("linked_modules --details -n=0 --json --relative", 2000, 15000, is_open_details)
        for ent in ret["entries"]:
            assert not ent["filename"].startswith(nfsdb.source_root)

    def test_details_relative_raw(self):
        ret = get_raw("linked_modules --details --relative", 2000, 15000, is_raw_open_details)
        for ent in ret:
            assert not ent.split(RAW_SEP)[0].startswith(nfsdb.source_root)

    def test_count(self):
        assert get_json_entries("linked_modules --json ")["count"] == \
            get_json_simple("linked_modules --json -l")["count"]


class TestModdepsFor:
    def test_required_param(self):
        get_error("moddeps_for", error_keyword="Missing required args 'path'")

    def test_plain(self, vmlinux):
        entr = get_json_entries(f"moddeps_for --path={vmlinux} -n=0 --json", 400, 3000, is_normpath)
        assert vmlinux not in entr['entries']

    def test_vs_deps(self, vmlinux):
        md_entr = get_json_entries(f"moddeps_for --path={vmlinux} -n=0 --sorted --json", 400, 3000, is_normpath)
        de_entr = get_json_entries(f"deps_for --path={vmlinux} --filter=[class=linked] -n=0 --sorted --json", 400, 3000, is_normpath)
        assert len(md_entr['entries']) == len(de_entr['entries']) and set(md_entr['entries']) == set(de_entr['entries'])

    def test_vs_deps_raw(self, vmlinux):
        md_entr = get_raw(f"moddeps_for --path={vmlinux} -n=0 --sorted", 400, 3000, is_normpath)
        de_entr = get_raw(f"deps_for --path={vmlinux} --filter=[class=linked] -n=0 --sorted", 400, 3000, is_normpath)
        assert len(md_entr) == len(de_entr) and set(md_entr) == set(de_entr)

    def test_plain_raw(self, vmlinux):
        get_raw(f"moddeps_for --path={vmlinux}", 400, 3000, is_normpath)

    def test_relative(self, vmlinux):
        ret = get_json_entries(f"moddeps_for --path={vmlinux} -n=0 --json --relative", 400, 3000, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_relative_raw(self, vmlinux):
        ret = get_raw(f"moddeps_for --path={vmlinux} --relative", 400, 3000, is_normpath)
        for ent in ret:
            assert not ent.startswith(nfsdb.source_root)

    def test_plain_direct(self, vmlinux):
        get_json_entries(f"moddeps_for --path={vmlinux} -n=0 --direct --json", 20, 2000, is_normpath)

    def test_plain_direct_raw(self, vmlinux):
        get_raw(f"moddeps_for --path={vmlinux} --direct", 20, 2000, is_normpath)

    def test_filter(self, vmlinux):
        get_json_entries(f"moddeps_for --path={vmlinux} --filter=[path=*.o,type=wc] -n=0 --json", 10, 2000, is_normpath)

    def test_filter_raw(self, vmlinux):
        get_raw(f"moddeps_for --path={vmlinux} --filter=[path=*.o,type=wc]", 10, 2000, is_normpath)

    def test_commands(self, vmlinux):
        get_json_entries(f"moddeps_for --path={vmlinux} --commands -n=0 --json", 400, 5000, is_command)

    def test_commands_raw(self, vmlinux):
        get_raw(f"moddeps_for --path={vmlinux} --commands", 400, 5000, is_raw_command)

    def test_details(self, vmlinux):
        get_json_entries(f"moddeps_for --path={vmlinux} --details -n=0 --json", 400, 5000, is_open_details)

    def test_details_raw(self, vmlinux):
        get_raw(f"moddeps_for --path={vmlinux} --details", 400, 5000, is_raw_open_details)

    def test_details_relative(self, vmlinux):
        ret = get_json_entries(f"moddeps_for --path={vmlinux} --details -n=0 --json --relative", 400, 5000, is_open_details)
        for ent in ret["entries"]:
            assert not ent["filename"].startswith(nfsdb.source_root)

    def test_details_relative_raw(self, vmlinux):
        ret = get_raw(f"moddeps_for --path={vmlinux} --details --relative", 400, 5000, is_raw_open_details)
        for ent in ret:
            assert not ent.split(RAW_SEP)[0].startswith(nfsdb.source_root)

    def test_generate(self, vmlinux):
        get_json_simple(f"moddeps_for --path={vmlinux} --commands --generate --all -n=0 --json", 400, 3000, is_generated)

    def test_generate_raw(self, vmlinux):
        get_raw(f"moddeps_for --path={vmlinux} --commands --generate --all", 400, 3000, is_raw_generated)

    def test_count(self, vmlinux):
        assert get_json_entries(f"moddeps_for --path={vmlinux} --json ", 400, 3000)["count"] == \
            get_json_simple(f"moddeps_for --path={vmlinux} --json -l")["count"]

    def test_pipeline(self, vmlinux):
        entr = get_json_entries("linked_modules --filter=[path=*vmlinux,type=wc] moddeps_for -n=0 --json", 400, 3000, is_normpath)
        assert vmlinux not in entr['entries']


class TestProcref:
    def test_empty(self):
        get_error("procref --pid=-1 -n=1", error_keyword="Invalid pid key")

    def test_plain(self, pids):
        get_json_entries(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --json", 1, 2000, is_normpath)

    def test_plain_raw(self, pids):
        get_raw(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]}", 1, 2000, is_normpath)

    def test_details(self, pids):
        ret = get_json_entries(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --details --json", 1, 2000, is_open_details)
        for ent in ret["entries"]:
            assert ent["ppid"] in pids

    def test_details_raw(self, pids):
        ret = get_raw(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --details", 1, 2000, is_raw_open_details)
        for ent in ret:
            assert int(ent.split(RAW_SEP)[2]) in pids

    def test_commands(self, pids):
        ret = get_json_entries(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --commands --json", 1, 2000)
        for ent in ret["entries"]:
            assert ent["pid"] in pids

    def test_commands_raw(self, pids):
        ret = get_raw(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --commands", 1, 2000, is_raw_command)
        for ent in ret:
            assert int(ent.split(RAW_SEP)[0]) in pids

    def test_relative(self, pids):
        ret = get_json_entries(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --json --relative", 1, 2000, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_relative_raw(self, pids):
        ret = get_raw(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --relative", 1, 2000, is_normpath)
        for ent in ret:
            assert not ent.startswith(nfsdb.source_root)

    def test_details_relative(self, pids):
        ret = get_json_entries(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --details --json --relative", 1, 2000, is_open_details)
        for ent in ret["entries"]:
            assert ent["ppid"] in pids and not ent["filename"].startswith(nfsdb.source_root)

    def test_details_raw_relative(self, pids):
        ret = get_raw(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --details --relative", 1, 2000, is_raw_open_details)
        for ent in ret:
            assert int(ent.split(RAW_SEP)[2]) in pids and not ent.split(RAW_SEP)[0].startswith(nfsdb.source_root)

    def test_count(self, pids):
        assert get_json_entries(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --json")["count"] == \
            get_json_simple(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --json -l")["count"]


class TestRefFiles:
    def test_plain(self):
        get_json_entries("ref_files -n=1000 --json", 1000000, 6000000, is_normpath)

    def test_plain_raw(self):
        get_raw("ref_files -n=1000", 1000000, 6000000, is_normpath)

    def test_with_filter(self):
        ret = get_json_entries("ref_files --filter=[path=*.c,type=wc,exists=1,global_access=r] -n=1000 --json", 8000, 50000, is_normpath)
        for ent in ret["entries"]:
            assert ent.endswith(".c")

    def test_with_filter_raw(self):
        ret = get_raw("ref_files --filter=[path=*.c,type=wc,exists=1,global_access=r]", 8000, 50000, is_normpath)
        for ent in ret:
            assert ent.endswith(".c")

    def test_details(self):
        get_json_entries("ref_files --details -n=1000 --json", 10000000, 30000000, is_open_details)

    def test_details_filtered(self):
        ret = get_json_entries("ref_files --filter=[path=*.c,type=wc,exists=1,access=r] --details -n=100 --json", 10000, 250000, is_open_details, timeout=10)
        for ent in ret["entries"]:
            assert ent["filename"].endswith(".c") and ent["access"] == "r"

    def test_details_raw(self):
        ret = get_raw("ref_files --filter=[path=*.c,type=wc,exists=1,access=r] --details -n=100", 10000, 250000, is_raw_open_details, timeout=25)
        for ent in ret:
            assert ent.split(RAW_SEP)[0].endswith(".c") and ent.split(RAW_SEP)[4] == "r"

    def test_compile_commands(self):
        get_json_entries("ref_files --filter=[path=*.c,type=wc,exists=1] --commands -n=100 --json", 20000, 50000, is_command, timeout=10)

    def test_compile_commands_generate(self):
        get_json_simple("ref_files --filter=[path=*.c,type=wc,exists=1] --commands --generate -n=0 --json", 20000, 50000, is_generated, timeout=10)

    def test_compile_commands_cmd_filter(self):
        get_json_entries("ref_files --filter=[path=*.c,type=wc,exists=1] --command-filter=[bin=*clang*,type=wc] --commands -n=100 --json", 20000, 50000, is_command, timeout=10)

    def test_compile_commands_raw(self):
        get_raw("ref_files --filter=[path=*.c,type=wc,exists=1] --commands -n=100", 10000, 50000, is_raw_command, timeout=10)

    def test_rdm(self):
        get_rdm("ref_files --filter=[path=*.c,type=wc,exists=1] --rdm --json -n=0", 10000, 50000, is_rdm)

    def test_rdm_raw(self):
        get_raw("ref_files --filter=[path=*.c,type=wc,exists=1] --rdm -n=0", 10000, 80000, is_raw_rdm)

    def test_relative(self):
        ret = get_json_entries("ref_files --json --relative -n=1000", 1000000, 6000000, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_relative_raw(self):
        ret = get_raw("ref_files --relative  -n=1000", 1000000, 6000000, is_normpath)
        for ent in ret:
            assert not ent.startswith(nfsdb.source_root)

    def test_count(self):
        assert get_json_entries("ref_files --json ")["count"] == \
            get_json_simple("ref_files --json -l")["count"]

    def test_filter_re_wc(self):
        re_results = get_json_entries("ref_files --filter=[path=.*\\.cc$,type=re] --json -n=0")
        wc_results = get_json_entries("ref_files --filter=[path=*.cc,type=wc] --json -n=0")
        assert len(re_results["entries"]) == len(wc_results["entries"])
        for x in range(0, len(re_results["entries"])):
            assert re_results["entries"][x] == wc_results["entries"][x]

    def test_filter_count_class(self, ref_files_len):

        linked_len = get_json_simple("ref_files --filter=[class=linked] --details --json -l", timeout=15)['count']
        le_len = get_json_simple("ref_files --filter=[class=linked_exe] --details --json -l", timeout=15)['count']
        lt_len = get_json_simple("ref_files --filter=[class=linked_static] --details --json -l", timeout=15)['count']
        lh_len = get_json_simple("ref_files --filter=[class=linked_shared] --details --json -l", timeout=15)['count']
        assert linked_len == (le_len + lt_len + lh_len)

        nl_len = get_json_simple("ref_files --filter=[class=linked,negate=1] --details --json -l", timeout=15)['count']
        assert ref_files_len == (linked_len + nl_len)

        compiled_len = get_json_simple("ref_files --filter=[class=compiled] --details --json -l", timeout=15)['count']
        n_comp_len = get_json_simple("ref_files --filter=[class=compiled,negate=1] --details --json  -l", timeout=15)['count']
        assert ref_files_len == (compiled_len + n_comp_len)

        plain_len = get_json_simple("ref_files --filter=[class=plain] --details --json -l", timeout=15)['count']
        n_plain_len = get_json_simple("ref_files --filter=[class=plain,negate=1] --details --json  -l", timeout=15)['count']
        assert ref_files_len == (plain_len + n_plain_len)

        binary_len = get_json_simple("ref_files --filter=[class=binary] --details --json -l", timeout=15)['count']
        n_binary_len = get_json_simple("ref_files --filter=[class=binary,negate=1] --details --json  -l", timeout=15)['count']
        assert ref_files_len == (binary_len + n_binary_len)

        # linker_len = get_json_simple("ref_files --filter=[class=linker] --details --json -l")['count']
        # assert linker_len > 0

        # compiler_len = get_json_simple("ref_files --filter=[class=compiler] --details --json -l")['count']
        # assert compiler_len > 0
        summarized = linked_len + compiled_len + plain_len + binary_len
        assert ref_files_len == summarized

    def test_filter_count_exist(self, ref_files_len):
        fil_ex_len = get_json_simple("ref_files --filter=[exists=1] --details --json -l")['count']
        dir_ex_len = get_json_simple("ref_files --filter=[exists=2] --details --json -l")['count']
        n_ex_len = get_json_simple("ref_files --filter=[exists=0] --details --json -l")['count']
        assert ref_files_len == (fil_ex_len + dir_ex_len + n_ex_len)

    def test_filter_count_source_root(self, ref_files_len):
        sr_len = get_json_simple("ref_files --filter=[source_root=1] --details --json -l")['count']
        nsr_len = get_json_simple("ref_files --filter=[source_root=0] --details --json -l")['count']
        assert ref_files_len == (sr_len + nsr_len)

    def test_filter_count_symlink(self):
        fil_ex_len = get_json_simple("ref_files --filter=[exists=1] --details --json -l")['count']
        sl_len = get_json_simple("ref_files --filter=[exists=1,link=1] --details --json -l")['count']
        nsl_len = get_json_simple("ref_files --filter=[exists=1,link=0] --details --json -l")['count']
        assert fil_ex_len == (sl_len + nsl_len)

    def test_filter_count_access(self, ref_files_len):
        r_len = get_json_simple("ref_files --filter=[access=r] --details --json -l")['count']
        w_len = get_json_simple("ref_files --filter=[access=w] --details --json -l")['count']
        rw_len = get_json_simple("ref_files --filter=[access=rw] --details --json -l")['count']
        assert ref_files_len == (r_len + w_len + rw_len)

    def test_filter_count_source_type(self):
        compiled_len = get_json_simple("ref_files --filter=[class=compiled] --details --json -l", timeout=5)['count']
        c_len = get_json_simple("ref_files --filter=[source_type=c] --details --json -l", timeout=5)['count']
        cpp_len = get_json_simple("ref_files --filter=[source_type=c++] --details --json -l", timeout=5)['count']
        other_len = get_json_simple("ref_files --filter=[source_type=other] --details --json -l", timeout=5)['count']
        assert compiled_len == (c_len + cpp_len + other_len)


class TestRevCompsFor:
    def test_plain(self, header_file):
        ret = get_json_entries(f"revcomps_for --path={header_file} --json", 1, 200, is_normpath)
        for ent in ret["entries"]:
            assert is_compiled(ent)

    def test_plain_raw(self, header_file):
        ret = get_raw(f"revcomps_for --path={header_file}", 1, 200, is_normpath)
        for ent in ret:
            assert is_compiled(ent)

    def test_filter(self, header_file):
        ret = get_json_entries(f"revcomps_for --path={header_file} --filter=[path=*.cc,type=wc] --json", 1, 200, is_normpath)
        for ent in ret["entries"]:
            assert ent.endswith(".cc")

    def test_filter_raw(self, header_file):
        ret = get_raw(f"revcomps_for --path={header_file} --filter=[path=*.cc,type=wc]", 1, 200, is_normpath)
        for ent in ret:
            assert ent.endswith(".cc")

    def test_filter_details(self, header_file):
        ret = get_json_entries(f"revcomps_for --path={header_file} --filter=[path=*.cc,type=wc] --details --json", 1, 200, is_command)
        for ent in ret["entries"]:
            for dk in ent['compiled']:
                assert dk.endswith(".cc") and ent['compiled'][dk] == "c++"

    def test_filter_details_raw(self, header_file):
        ret = get_raw(f"revcomps_for --path={header_file} --filter=[path=*.cc,type=wc] --details ", 1, 200, is_raw_command)
        for ent in ret:
            src = ent.split(RAW_SEP)[8].split(":")
            assert src[0].endswith(".cc") and src[1] == "c++"

    def test_filter_cmd(self, header_file):
        ret = get_json_entries(f"revcomps_for --path={header_file} --filter=[path=*.cc,type=wc] --commands --json", 1, 200, is_command)
        for ent in ret["entries"]:
            for dk in ent['compiled']:
                assert dk.endswith(".cc") and ent['compiled'][dk] == "c++"

    def test_filter_cmd_raw(self, header_file):
        ret = get_raw(f"revcomps_for --path={header_file} --filter=[path=*.cc,type=wc] --commands ", 1, 200, is_raw_command)
        for ent in ret:
            src = ent.split(RAW_SEP)[8].split(":")
            assert src[0].endswith(".cc") and src[1] == "c++"

    def test_relative(self, header_file):
        ret = get_json_entries(f"revcomps_for --path={header_file} --filter=[path=*.c,type=wc]or[path=*.cc,type=wc] --relative --json", 1, 200, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_relative_raw(self, header_file):
        ret = get_raw(f"revcomps_for --path={header_file} --filter=[path=*.c,type=wc]or[path=*.cc,type=wc] --relative", 1, 200, is_normpath)
        for ent in ret:
            assert not ent.startswith(nfsdb.source_root)

    def test_count(self, header_file):
        assert get_json_entries(f"revcomps_for --path={header_file} --json ")["count"] == \
            get_json_simple(f"revcomps_for --path={header_file} --json -l")["count"]


class TestRevDepsFor:
    def test_plain(self, compiled_file):
        get_json_entries(f"revdeps_for --path={compiled_file} --json", 1, 1000, is_normpath)

    def test_plain_raw(self, compiled_file,):
        get_raw(f"revdeps_for --path={compiled_file}", 1, 1000, is_normpath)

    def test_plain_multiple(self, compiled_file, header_file):
        get_json_entries(f"revdeps_for --path={compiled_file} --path={header_file} --json", 1, 1000, is_normpath)

    def test_plain_multiple_raw(self, compiled_file, header_file):
        get_raw(f"revdeps_for --path={compiled_file} --path={header_file}", 1, 1000, is_normpath)

    def test_recursive(self, compiled_file):
        r_normal = get_json_entries(f"revdeps_for --path={compiled_file} --json", 1, 1000, is_normpath)
        r_recursive = get_json_entries(f"revdeps_for --path={compiled_file} --recursive --json", 1, 1000, is_normpath)
        assert r_normal["count"] < r_recursive["count"]

    def test_recursive_raw(self, compiled_file):
        r_normal = get_raw(f"revdeps_for --path={compiled_file}", 1, 1000, is_normpath)
        r_recursive = get_raw(f"revdeps_for --path={compiled_file} --recursive", 1, 1000, is_normpath)
        assert len(r_normal) < len(r_recursive)

    def test_filter(self, compiled_file):
        get_json_entries(f"revdeps_for --path={compiled_file} --filter=[path=*curl*,type=wc] --json", 1, 20, is_normpath)

    def test_filter_raw(self, compiled_file):
        get_raw(f"revdeps_for --path={compiled_file} --filter=[path=*curl*,type=wc]", 1, 20, is_normpath)

    def test_filter_commands(self, compiled_file):
        ret = get_json_entries(f"revdeps_for --path={compiled_file} --filter=[path=*curl*,type=wc] --commands --command-filter=[bin=*ld.lld,type=wc] --json", 1, 20, is_command)
        for ent in ret['entries']:
            assert ent['bin'].endswith("ld.lld")

    def test_filter_commands_raw(self, compiled_file):
        ret = get_raw(f"revdeps_for --path={compiled_file} --filter=[path=*curl*,type=wc] --commands --command-filter=[bin=*ld.lld,type=wc]", 1, 20, is_raw_command)
        for ent in ret:
            assert ent.split(RAW_SEP)[4].endswith("ld.lld")

    def test_relative(self, compiled_file):
        ret = get_json_entries(f"revdeps_for --path={compiled_file} --filter=[path=*curl*,type=wc] --json --relative", 1, 20, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_relative_raw(self, compiled_file):
        ret = get_raw(f"revdeps_for --path={compiled_file} --filter=[path=*curl*,type=wc] --relative", 1, 20, is_normpath)
        for ent in ret:
            assert not ent.startswith(nfsdb.source_root)

    def test_cdm(self, compiled_file):
        get_rdm(f"revdeps_for --path={compiled_file} --json --cdm -n=0", 1, 1000, is_cdm)

    def test_cdm_raw(self, compiled_file):
        get_raw(f"revdeps_for --path={compiled_file} --cdm", 1, 10000, is_raw_cdm)

    def test_count(self, compiled_file):
        assert get_json_entries(f"revdeps_for --path={compiled_file} --json ")["count"] == \
            get_json_simple(f"revdeps_for --path={compiled_file} --json -l")["count"]

    def test_pipeline(self):
        get_json_entries("compiled --filter=[path=*curl/lib/md5.c,type=wc] revdeps_for -n=0 --json --match", 1, 1000, is_normpath)


class TestPipeline:
    def test_binaries_2_commands(self):
        ret = get_json_entries("binaries --command-filter=[bin=*javac,type=wc] commands --json", 300, 1500, is_command)
        for ent in ret["entries"]:
            assert fnmatch.fnmatch(ent['bin'], "*javac")
        count = get_json_simple("binaries -l --command-filter=[bin=*javac,type=wc] commands --json")["count"]
        assert ret["count"] == count

    def test_linked_modules_2_deps_for(self):
        ret = get_json_entries("linked_modules --filter=[path=*vmlinux,type=wc] deps_for --json", 13000, 25000, is_normpath)
        count = get_json_simple("linked_modules -l --filter=[path=*vmlinux,type=wc] deps_for --json")["count"]
        assert ret["count"] == count


class TestMiscModules:
    def test_version(self):
        ret = get_json_simple("version --json")
        assert ret["version"] == nfsdb.get_version()

    def test_version_raw(self):
        ret = get_raw("version")
        assert ret[0] == nfsdb.get_version()

    def test_compiler_pattern(self):
        get_json_simple("compiler_pattern --json")

    def test_compiler_pattern_raw(self):
        get_json_simple("compiler_pattern")

    def test_linker_pattern(self):
        get_json_simple("linker_pattern --json")

    def test_linker_pattern_raw(self):
        get_json_simple("linker_pattern")

    def test_root_pid(self):
        ret = get_json_simple("root_pid --json")
        assert isinstance(ret["root_pid"], int)

    def test_root_pid_raw(self):
        get_pid("root_pid")

    def test_source_root(self):
        ret = get_json_simple("source_root --json")
        assert ret["source_root"] == nfsdb.source_root

    def test_source_root_raw(self):
        ret = get_raw("source_root")
        assert ret[0] == nfsdb.source_root

    def test_show_config(self):
        get_json_simple("config --json")

    def test_show_config_raw(self):
        get_raw("config")

    def test_show_stats(self):
        get_json_simple("stat --json")

    def test_stats_raw(self):
        get_raw("stat")
