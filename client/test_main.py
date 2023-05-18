# pylint: disable=missing-function-docstring missing-class-docstring too-many-lines missing-module-docstring static
import os
import fnmatch
import sys
import json
from typing import Iterator, Generator
import pytest
import libcas

from client.cmdline import process_commandline
from client.argparser import get_api_modules, get_args, merge_args
from client.filtering import Filter, FilterException

DEF_TIMEOUT = 25
RAW_SEP = "^^^"

nfsdb = libcas.CASDatabase()
config = libcas.CASConfig(os.path.join(os.environ["DB_DIR"], ".bas_config"))
nfsdb.set_config(config)
nfsdb.load_db(os.path.join(os.environ["DB_DIR"], ".nfsdb.img"))
nfsdb.load_deps_db(os.path.join(os.environ["DB_DIR"], ".nfsdb.deps.img"))


@pytest.fixture(name="vmlinux")
def fixture_vmlinux() -> str:
    return get_main("linked_modules -n=1 --filter=[path=*vmlinux,type=wc,exists=1]and[compressed,negate=true]")


@pytest.fixture(name="vmlinux_o")
def fixture_vmlinux_o() -> str:
    return get_main("linked_modules -n=1 --filter=[path=*vmlinux.o,type=wc,exists=1]")


@pytest.fixture(name="some_obj")
def fixture_some_obj() -> str:
    return get_main("linked_modules -n=1 --filter=[path=*/vgettimeofday.o,type=wc]")


@pytest.fixture(name="compiled_file")
def fixture_compiled_file() -> str:
    return get_main("compiled --filter=[path=*curl/lib/md5.c,type=wc] -n=1")


@pytest.fixture(name="header_file")
def fixture_header_file() -> str:
    return get_main("linked_modules --filter=[path=*libbtdevice.a,type=wc] deps_for --filter=[path=*android/packages/modules/Bluetooth/system/osi/include/list.h,type=wc]")


@pytest.fixture(name="javac")
def fixture_javac() -> str:
    return get_main("binaries --filter=[bin=*prebuilts/jdk/jdk11/linux-x86/bin/javac,type=wc] -n=1 --reverse")


@pytest.fixture(name="sh")
def fixture_sh() -> str:
    return get_main("binaries --filter=[bin=*bin/sh,type=wc] -n=1")


@pytest.fixture(name="bash")
def fixture_bash() -> str:
    return get_main("binaries --filter=[bin=*bin/bash,type=wc] -n=1")


@pytest.fixture(name="linker")
def fixture_linker() -> str:
    return get_main("binaries --filter=[bin=*/bin/ld.lld,type=wc] -n=1")


@pytest.fixture(name="pids")
def fixture_pids() -> list:
    return [int(x) for x in get_main("compiled --filter=[exists=1] --commands -n=3 --entry-fmt={p}").split(os.linesep)]


@pytest.fixture(name="ref_files_len")
def fixture_ref_files_len() -> list:
    return get_json_simple("ref_files --json -l")['count']

# @pytest.fixture(name="source_root")
# def fixture_source_root() -> str:
#     return get_main("source_root")


def get_json_entries(cmd, len_min=0, len_max=sys.maxsize, check_func=None):
    if "DB_DIR" in os.environ:
        cmd += " --dir={}".format(os.environ["DB_DIR"])
    j = get_main(cmd)
    ret = json.loads(j)
    assert "entries" in ret and "count" in ret and "page" in ret and "num_entries" in ret
    assert len_min <= int(ret["num_entries"]) <= len_max
    if check_func is not None:
        for ent in ret["entries"]:
            assert check_func(ent)
    return ret


def get_error(cmd, error_keyword):
    ret = get_main(cmd)
    assert "ERROR" in ret and error_keyword in ret
    return ret


def get_json_simple(cmd, len_min=0, len_max=sys.maxsize, check_func=None):
    ret = json.loads(get_main(cmd))
    assert "entries" not in ret
    assert len_min <= len(ret) <= len_max
    if check_func is not None:
        for ent in ret:
            assert check_func(ent)
    return ret


def get_rdm(cmd, len_min=0, len_max=sys.maxsize, check_func=None):
    ret = json.loads(get_main(cmd))
    assert len_min <= len(ret.keys()) <= len_max
    if check_func is not None:
        assert check_func(ret)
    return ret


def get_dep_graph(cmd, len_min=0, len_max=sys.maxsize, check_func=None):
    ret = json.loads(get_main(cmd))
    assert "dep_graph" in ret
    assert len_min <= len(ret["dep_graph"]) <= len_max
    if check_func is not None:
        assert check_func(ret)
    return ret


def get_raw(cmd, line_count_min=0, line_count_max=sys.maxsize, check_func=None):
    cmd = cmd + f" --separator={RAW_SEP}"
    ret = get_main(cmd)
    try:
        json.loads(ret)
        assert False
    except json.JSONDecodeError:
        pass
    out = ret.split(os.linesep)
    assert line_count_min <= len(out) <= line_count_max
    if check_func is not None:
        for ent in out:
            assert check_func(ent)
    return ret


def get_pid(cmd):
    cmd = cmd + f" --separator={RAW_SEP}"
    ret = get_main(cmd)
    try:
        ret = int(ret)
    except ValueError:
        assert False
    return ret


def get_main(cmd):
    ret = process_commandline(nfsdb, cmd + " --sorted")
    if ret:
        if isinstance(ret, Iterator):
            return os.linesep.join(ret)
        elif isinstance(ret, Generator):
            return os.linesep.join(ret)
        else:
            return ret
    return ""


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


def is_raw_comp_info(obj):
    rows = obj.split(os.linesep)
    return rows[0] == 'COMPILATION:' and rows[1].startswith('  PID: ')


def is_normpath(obj):
    return isinstance(obj, str) and (os.path.normpath(obj) == obj or obj == '')


def is_linked(obj):
    return obj.endswith(".so") or obj.endswith(".so.raw") or obj.endswith(".so.dbg") \
           or obj.endswith(".a") or obj.endswith(".o") or obj.endswith(".ko") or obj.endswith("vmlinux") or obj.endswith(".btf") \
           or obj.endswith(".elf")


def is_compiled(obj):
    return obj.endswith(".c") or obj.endswith(".c.dist") or obj.endswith(".cc") or obj.endswith(".cpp") or obj.endswith(".cxx") \
           or obj.endswith(".h") or obj.endswith(".S") or obj.endswith(".s") or obj.endswith(".o")


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
    return isinstance(obj, dict) and "directory" in obj and "command" in obj


def is_raw_generated(obj):
    row = obj.split(RAW_SEP)
    return len(row) == 3


def is_raw_generated_openrefs(obj):
    row = obj.split(RAW_SEP)
    return len(row) == 4


def is_generated_openrefs(obj):
    return isinstance(obj, dict) and "directory" in obj and "command" in obj and "openfiles" in obj


def is_makefile(obj):
    return obj.startswith(".PHONY: all")


def is_rdm(obj):
    return isinstance(obj, dict) and isinstance(next(iter(obj)), str) and \
        isinstance(obj[next(iter(obj))], list) and \
        isinstance(obj[next(iter(obj))][0], str)


def is_cdm(obj):
    return is_rdm(obj)


def is_dep_graph(obj):
    return isinstance(obj, dict) and "dep_graph" in obj and \
        isinstance(obj["dep_graph"][next(iter(obj["dep_graph"]))], list) and \
        isinstance(obj["dep_graph"][next(iter(obj["dep_graph"]))][0][0][0], int)


def is_raw_dep_graph(obj):
    rows = obj[0:10000].split(os.linesep)
    return os.path.normpath(rows[0]) and \
        "PIDS:" in rows[1]


def is_raw_rdm(obj):
    rows = obj.split(os.linesep)
    for row in rows:
        assert os.path.normpath(row.replace("  /", "/"))
    return True


def is_raw_cdm(obj):
    return is_raw_rdm(obj)


@pytest.mark.timeout(DEF_TIMEOUT)
class TestGeneric:
    def trigger_exception(self, args, assert_message):
        try:
            common_args, pipeline_args, _ = get_args(args)
            parsed_args = merge_args(common_args, pipeline_args[0])
            Filter(parsed_args.filter, None, None, None)
            assert False
        except FilterException as err:
            if assert_message in err.message:
                assert True
            else:
                assert False

    def get_parsed_filter(self, args):
        try:
            common_args, pipeline_args, _ = get_args(args)
            parsed_args = merge_args(common_args, pipeline_args[0])
            return Filter(parsed_args.filter, None, None, None)
        except FilterException:
            assert False

    def test_arg_parser(self):
        args = ["deps_for", "--path", "aaa", "--path", "bbb"]
        common_args, pipeline_args, _ = get_args(args)
        parsed_args = merge_args(common_args, pipeline_args[0])
        assert parsed_args.module == get_api_modules().get(args[0].replace("--", ""), None)
        assert len(parsed_args.path) == 2 and parsed_args.path[0] == args[2] and parsed_args.path[1] == args[4]

    def test_filter_no_module(self):
        self.trigger_exception(["linked_modules", "--filter=[bad_key=bad_value]"], "Unknown filter parameter bad_key")

    def test_filter_bad_key(self):
        self.trigger_exception(["linked_modules", "--filter=[bad_key=bad_value]"], "Unknown filter parameter bad_key")

    def test_filter_dict(self):
        Filter([[{"path": "aa"}, {"class": "compiled"}], [{"path": "bb"}]], None, None, None)

    def test_filter_dict_bad(self):
        try:
            Filter([[{"path": "aa"}, {"class": "compiled"}], [{"path": "bb"}], [{"bad_path": "aa"}]], None, None, None)
        except FilterException as err:
            if "Unknown filter parameter bad_path" in err.message:
                assert True
            else:
                assert False

    def test_filter_bad_value(self):
        self.trigger_exception(["linked_modules", "--filter=[path=/some_file,type=bad_value]or[path=/some_file,type=re]"], "parameter value")

    def test_filter_missing_primary_key(self):
        self.trigger_exception(["linked_modules", "--filter=[type=re]"], "sub-parameter present but without associated typed parameter")

    def test_filter_multiple_primary_key(self):
        self.trigger_exception(["linked_modules", "--filter=[cwd=/1,path=/2]"], "More than one typed parameter")

    def test_filter_path(self):
        fil = self.get_parsed_filter(["linked_modules", "--filter=[path=/abc,type=wc]"])
        assert len(fil.filter_dict) == 1 and len(fil.filter_dict[0]) == 1
        assert fil.filter_dict[0][0]["path"] == "/abc" and "path_pattern" in fil.filter_dict[0][0]

    def test_filter_path_anded(self):
        fil = self.get_parsed_filter(["linked_modules", "--filter=[path=/abc,type=wc]and[path=/def,type=wc]"])
        assert len(fil.filter_dict) == 1 and len(fil.filter_dict[0]) == 2
        assert fil.filter_dict[0][0]["path"] == "/abc" and "path_pattern" in fil.filter_dict[0][0]
        assert fil.filter_dict[0][1]["path"] == "/def" and "path_pattern" in fil.filter_dict[0][1]

    def test_filter_path_ored(self):
        fil = self.get_parsed_filter(["linked_modules", "--filter=[path=/abc,type=wc]and[path=/123,type=wc]or[path=/def,type=wc]and[path=/ghi,type=wc]and[path=/jkl,type=wc]"])
        assert len(fil.filter_dict) == 2 and len(fil.filter_dict[0]) == 2 and len(fil.filter_dict[1]) == 3
        assert fil.filter_dict[0][0]["path"] == "/abc" and "path_pattern" in fil.filter_dict[0][0]
        assert fil.filter_dict[0][1]["path"] == "/123" and "path_pattern" in fil.filter_dict[0][1]
        # OR
        assert fil.filter_dict[1][0]["path"] == "/def" and "path_pattern" in fil.filter_dict[1][0]
        assert fil.filter_dict[1][1]["path"] == "/ghi" and "path_pattern" in fil.filter_dict[1][1]
        assert fil.filter_dict[1][2]["path"] == "/jkl" and "path_pattern" in fil.filter_dict[1][2]


@pytest.mark.timeout(DEF_TIMEOUT)
class TestBinaries:
    def test_simple(self):
        get_json_entries("binaries -n=0 --json", 1000, 3000, is_normpath)

    def test_simple_raw(self):
        get_raw("binaries", 1000, 3000, is_normpath)

    def test_simple_relative(self):
        ret = get_json_entries("binaries -n=0 --json --relative", 1000, 3000, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_with_bin(self):
        ret = get_json_entries("binaries --filter=[bin=/bin/bash]or[bin=/bin/sh] --json", 2, 2, is_normpath)
        for ent in ret["entries"]:
            assert fnmatch.fnmatch(ent, "*/bin/*sh")

    def test_with_bin_raw(self):
        ret = get_raw("binaries --filter=[bin=/bin/bash]or[bin=/bin/sh]", 2, 2, is_normpath)
        for ent in ret.split(os.linesep):
            assert fnmatch.fnmatch(ent, "*/bin/*sh")

    def test_with_path_details(self, javac):
        ret = get_json_entries(f"binaries -n=100 --filter=[bin={javac}] --details --json", 600, 1500, is_command)
        for ent in ret["entries"]:
            assert ent['bin'] == javac

    def test_with_path_details_raw(self, javac):
        ret = get_raw(f"binaries -n=100 --filter=[bin={javac}] --details", 100, 100, is_raw_command)
        for ent in ret.split(os.linesep):
            assert javac in ent

    def test_with_filter(self):
        ret = get_json_entries("binaries -n=10 --filter=[bin=*.py,type=wc,exists=1] --json", 50, 150, is_normpath)
        for ent in ret["entries"]:
            assert fnmatch.fnmatch(ent, "*.py")

    def test_with_filter_raw(self):
        ret = get_raw("binaries --filter=[bin=*.py,type=wc,exists=1]", 50, 150, is_normpath)
        for ent in ret.split(os.linesep):
            assert fnmatch.fnmatch(ent, "*.py")

    def test_compiler(self):
        ret = get_json_entries("binaries -n=0 --filter=[class=compiler] --json", 5, 20, is_normpath)
        comp_wc = config.config_info["gcc_spec"] + config.config_info["gpp_spec"] + config.config_info["clang_spec"] + config.config_info["clangpp_spec"] + config.config_info["armcc_spec"]
        for ent in ret["entries"]:
            assert any([len(fnmatch.filter([ent], wc)) > 0 for wc in comp_wc])

    def test_compiler_raw(self):
        ret = get_raw("binaries --filter=[class=compiler]", 5, 20, is_normpath)
        comp_wc = config.config_info["gcc_spec"] + config.config_info["gpp_spec"] + config.config_info["clang_spec"] + config.config_info["clangpp_spec"] + config.config_info["armcc_spec"]
        for ent in ret.split(os.linesep):
            assert any([len(fnmatch.filter([ent], wc)) > 0 for wc in comp_wc])

    def test_linker(self):
        ret = get_json_entries("binaries -n=0 --filter=[class=linker] --json", 5, 20, is_normpath)
        link_wc = config.config_info["ld_spec"] + config.config_info["ar_spec"] + ["/bin/bash"]
        for ent in ret["entries"]:
            assert any([len(fnmatch.filter([ent], wc)) > 0 for wc in link_wc])

    def test_linker_raw(self):
        ret = get_raw("binaries -n=0 --filter=[class=linker]", 5, 20, is_normpath)
        link_wc = config.config_info["ld_spec"] + config.config_info["ar_spec"] + ["/bin/bash"]
        for ent in ret.split(os.linesep):
            assert any([len(fnmatch.filter([ent], wc)) > 0 for wc in link_wc])

    def test_details(self):
        ret = get_json_entries("binaries -n=100 --filter=[class=compiler] --details --json", 50000, 100000, is_command)
        for ent in ret["entries"]:
            assert ent["class"] == "compiler"

    def test_details_raw(self):
        ret = get_raw("binaries -n=0 --filter=[class=compiler] --details", 50000, 100000, is_raw_command)
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[8].split(":")[1].startswith("c")

    def test_compile_commands(self):
        ret = get_json_entries("binaries --filter=[bin=*clang,type=wc,class=compiler] --commands --json", 15000, 150000, is_command)
        for ent in ret["entries"]:
            assert ent["class"] == "compiler"
            assert ent["bin"].endswith("clang")

    def test_compile_commands_raw(self):
        ret = get_raw("binaries --filter=[bin=*clang,type=wc,class=compiler] --commands", 15000, 150000, is_raw_command)
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[4].endswith("clang")

    def test_linking_commands(self):
        ret = get_json_entries("binaries --filter=[bin=*ld.lld,type=wc,class=linker] --commands --json", 4000, 15000, is_command)
        for ent in ret["entries"]:
            assert ent["class"] == "linker"
            assert ent["bin"].endswith("ld.lld")

    def test_linking_commands_raw(self):
        ret = get_raw("binaries --filter=[bin=*ld.lld,type=wc,class=linker] --commands", 4000, 15000, is_raw_command)
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[4].endswith("ld.lld")

    def test_count(self):
        assert get_json_entries("binaries --json ")["num_entries"] == get_json_simple("binaries --json -l")["count"]

@pytest.mark.timeout(DEF_TIMEOUT)
class TestCommands:
    def test_plain(self):
        get_json_entries("commands --json", 500000, 2500000, is_command)

    def test_plain_raw(self):
        get_raw("commands -n=1000", 1000, 1000, is_raw_command)

    def test_with_filter(self):
        ret = get_json_entries("commands --filter=[bin=*prebuilts*javac*,type=wc] --json", 1000, 4000, is_command)
        for ent in ret["entries"]:
            assert ("javac" in ent["bin"] and "prebuilts" in ent["bin"])

    def test_with_filter_raw(self):
        ret = get_raw("commands --filter=[bin=*prebuilts*javac*,type=wc]", 1000, 4000, is_raw_command)
        for ent in ret.split(os.linesep):
            assert ("javac" in ent and "prebuilts" in ent)

    def test_details(self):
        ret = get_json_entries("commands --filter=[bin=*prebuilts*javac*,type=wc] --details --json", 1000, 4000, is_command)
        for ent in ret["entries"]:
            assert ("javac" in ent["bin"] and "prebuilts" in ent["bin"])

    def test_details_raw(self):
        ret = get_raw("commands --filter=[bin=*prebuilts*javac*,type=wc] --details", 1000, 4000, is_raw_command)
        for ent in ret.split(os.linesep):
            assert ("javac" in ent and "prebuilts" in ent)

    def test_generate(self):
        ret = get_json_simple("commands --filter=[bin=*clang*,type=wc] --generate --json", 1000, 1000, is_generated)
        for ent in ret:
            assert"clang" in ent["command"]

    def test_generate_raw(self):
        get_raw("commands --filter=[bin=*clang*,type=wc] --generate", 1000, 100000, is_raw_generated)

    def test_generate_makefile(self):
        ret = get_raw("commands --filter=[bin=*/bin/clang,type=wc] --generate --makefile --json")
        assert is_makefile(ret)

    def test_generate_makefile_raw(self):
        ret = get_raw("commands --filter=[bin=*/bin/clang,type=wc] --generate --makefile")
        assert is_makefile(ret)

    def test_generate_makefile_all(self):
        ret = get_raw("commands --filter=[bin=*java,type=wc] --generate --makefile --all --json")
        assert is_makefile(ret)

    def test_generate_makefile_all_raw(self):
        ret = get_raw("commands --filter=[bin=*java,type=wc] --generate --makefile --all")
        assert is_makefile(ret)

    def test_class_linker(self):
        ret = get_json_entries("commands --filter=[class=linker] --json", 5000, 15000, is_command)
        link_wc = config.config_info["ld_spec"] + config.config_info["ar_spec"] + ["/bin/bash"]
        for ent in ret['entries']:
            matched = [len(fnmatch.filter([ent['bin'].split()[0]], wc)) > 0 for wc in link_wc]
            if not any(matched):
                assert False

    def test_class_linker_raw(self):
        ret = get_raw("commands --filter=[class=linker]", 5000, 15000, is_raw_command)
        link_wc = config.config_info["ld_spec"] + config.config_info["ar_spec"] + ["/bin/bash"]
        for ent in ret.split(os.linesep):
            matched = [len(fnmatch.filter([ent.split(RAW_SEP)[4]], wc)) > 0 for wc in link_wc]
            if not any(matched):
                assert False

    def test_class_compiler(self):
        ret = get_json_entries("commands --filter=[class=compiler] --json", 50000, 150000, is_command)
        comp_wc = config.config_info["gcc_spec"] + config.config_info["gpp_spec"] + config.config_info["clang_spec"] + config.config_info["clangpp_spec"] + config.config_info["armcc_spec"]
        for ent in ret['entries']:
            matched = [len(fnmatch.filter([ent['bin'].split()[0]], wc)) > 0 for wc in comp_wc]
            if not any(matched):
                assert False

    def test_class_compiler_raw(self):
        ret = get_raw("commands --filter=[class=compiler]", 50000, 150000, is_raw_command)
        comp_wc = config.config_info["gcc_spec"] + config.config_info["gpp_spec"] + config.config_info["clang_spec"] + config.config_info["clangpp_spec"] + config.config_info["armcc_spec"]
        for ent in ret.split(os.linesep):
            matched = [len(fnmatch.filter([ent.split(RAW_SEP)[4]], wc)) > 0 for wc in comp_wc]
            if not any(matched):
                assert False

    def test_path_plain(self,sh, bash):
        ret = get_json_entries(f"commands --binary={sh} --binary={bash} --json", 2000, 2500000, is_command)
        for ent in ret["entries"]:
            assert ent["bin"] in ["/usr/bin/sh", "/usr/bin/bash"]

    def test_path_plain_raw(self,sh, bash):
        ret = get_raw(f"commands --binary={sh} --binary={bash}", 2000, 2500000, is_raw_command)
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[4] == sh or ent.split(RAW_SEP)[4] == bash

    def test_path_filter_cmd(self, sh, bash):
        ret = get_json_entries(f"commands --binary={sh} --binary={bash} --filter=[cmd=.*vmlinux.*,type=re] --json", 10, 100, is_command)
        for ent in ret["entries"]:
            assert (ent["bin"] == bash or ent["bin"] == sh) or "vmlinux" in ent["command"]

    def test_path_filter_cmd_raw(self, sh, bash):
        ret = get_raw(f"commands --binary={sh} --binary={bash} --filter=[cmd=.*vmlinux.*,type=re]", 10, 100, is_raw_command)
        for ent in ret.split(os.linesep):
            assert (ent.split(RAW_SEP)[4] == bash or ent.split(RAW_SEP)[4] == sh) and "vmlinux" in ent.split(RAW_SEP)[6]

    def test_path_filter_cwd(self, javac):
        ret = get_json_entries(f"commands --binary={javac} --filter=[cwd=*android*,type=wc] --json", 300, 1500, is_command)
        for ent in ret["entries"]:
            assert ent["bin"] == javac and "android" in ent["cwd"]

    def test_path_filter_cwd_raw(self, javac):
        ret = get_raw(f"commands --binary={javac} --filter=[cwd=*android*,type=wc]", 300, 1500, is_raw_command)
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[4] == javac and "android" in ent.split(RAW_SEP)[5]

    def test_path_filter_class(self,linker):
        ret = get_json_entries(f"commands --binary={linker} --filter=[class=linker] --json", 50, 5000, is_command)
        for ent in ret["entries"]:
            assert ent['linked'] is not None

    def test_path_filter_class_raw(self,linker):
        ret = get_raw(f"commands --binary={linker} --filter=[class=linker]", 50, 5000, is_raw_command)
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[4] == linker

    def test_count(self):
        assert get_json_entries("commands --json ")["num_entries"] == get_json_simple("commands --json -l")["count"]


@pytest.mark.timeout(DEF_TIMEOUT)
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

    def test_pipeline(self):
        ret = get_json_entries("compiled --filter=[path=*curl/lib*,type=wc] compilation_info_for -n=10 --json", 400, 1500, is_comp_info)
        for ent in ret["entries"]:
            for fil in ent["compiled_files"]:
                assert is_compiled(fil)

    def test_pipeline_raw(self):
        ret = get_raw("compiled --filter=[path=*curl/lib*,type=wc] compilation_info_for")
        assert is_raw_comp_info(ret)

    def test_count(self):
        assert get_json_entries("compiled --filter=[path=*curl/lib*,type=wc] compilation_info_for --json ")["num_entries"] == \
            get_json_simple("compiled --filter=[path=*curl/lib*,type=wc] compilation_info_for --json -l")["count"]


@pytest.mark.timeout(DEF_TIMEOUT)
class TestCompiled:
    def test_plain(self):
        ret = get_json_entries("compiled -n=0 --json", 20000, 80000, is_normpath)
        for ent in ret["entries"]:
            assert is_compiled(ent)

    def test_plain_raw(self):
        ret = get_raw("compiled", 20000, 80000, is_normpath)
        for ent in ret.split(os.linesep):
            assert is_compiled(ent)

    def test_plain_relative(self):
        ret = get_json_entries("compiled -n=0 --json --relative", 20000, 80000, is_normpath)
        for ent in ret["entries"]:
            assert is_compiled(ent) and not ent.startswith(nfsdb.source_root)

    def test_plain_relative_raw(self):
        ret = get_raw("compiled --relative", 20000, 80000, is_normpath)
        for ent in ret.split(os.linesep):
            assert is_compiled(ent) and not ent.startswith(nfsdb.source_root)

    def test_with_filter(self):
        ret = get_json_entries("compiled --filter=[path=*.c,type=wc,exists=1,access=r] --json", 8000, 25000, is_normpath)
        for ent in ret["entries"]:
            assert ent.endswith(".c")

    def test_with_filter_raw(self):
        ret = get_raw("compiled --filter=[path=*.c,type=wc,exists=1,access=r]", 8000, 25000, is_normpath)
        for ent in ret.split(os.linesep):
            assert ent.endswith(".c")

    def test_details(self):
        ret = get_json_entries("compiled --filter=[path=*.cpp,type=wc] --details --json", 20000, 50000, is_open_details)
        for ent in ret["entries"]:
            assert ent['filename'].endswith(".cpp")

    def test_details_raw(self):
        ret = get_raw("compiled --filter=[path=*.cpp,type=wc] --details", 20000, 50000, is_raw_open_details)
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[0].endswith(".cpp")

    def test_details_relative(self):
        ret = get_json_entries("compiled --filter=[path=*.cpp,type=wc] --details --json --relative", 20000, 50000, is_open_details)
        for ent in ret["entries"]:
            assert ent['filename'].endswith(".cpp") and not ent['filename'].startswith(nfsdb.source_root)

    def test_details_relative_raw(self):
        ret = get_raw("compiled --filter=[path=*.cpp,type=wc] --details --relative", 20000, 50000, is_raw_open_details)
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[0].endswith(".cpp") and not ent.split(RAW_SEP)[0].startswith(nfsdb.source_root)

    def test_compile_commands(self):
        ret = get_json_entries("compiled --commands --json -n=0", 50000, 150000, is_command)
        for ent in ret["entries"]:
            assert ent["class"] == "compiler"

    def test_compile_commands_raw(self):
        ret = get_raw("compiled --commands", 50000, 150000, is_raw_command)
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[8].split(":")[1].startswith("c")

    def test_relative(self):
        ret = get_json_entries("compiled --json --relative", 10000, 50000, is_normpath)
        s_r = get_main("source_root")
        for ent in ret["entries"]:
            assert not ent.startswith(s_r)

    def test_relative_raw(self):
        ret = get_raw("compiled --relative", 10000, 50000, is_normpath)
        s_r = get_main("source_root")
        for ent in ret.split(os.linesep):
            assert not ent.startswith(s_r)

    def test_count(self):
        assert get_json_entries("compiled --json ")["num_entries"] == \
            get_json_simple("compiled --json -l")["count"]


@pytest.mark.timeout(DEF_TIMEOUT)
class TestDepsFor:
    def test_required_param(self):
        get_error("deps_for", error_keyword="Missing required args 'path'")

    def test_plain(self, vmlinux):
        get_json_entries(f"deps_for --path={vmlinux} --json", 15000, 40000, is_normpath)

    def test_plain_raw(self, vmlinux):
        get_raw(f"deps_for --path={vmlinux}", 15000, 40000, is_normpath)

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
        for ent in ret.split(os.linesep):
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
        for ent in ret.split(os.linesep):
            assert not ent.startswith(nfsdb.source_root)

    def test_plain_epath(self, vmlinux):
        get_json_entries(f"deps_for --path=[file={vmlinux},direct=true] --json", 1000, 3000, is_normpath)

    def test_plain_epath_raw(self, vmlinux):
        get_raw(f"deps_for --path=[file={vmlinux},direct=true]", 1000, 3000, is_normpath)

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
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[0].endswith(".c")

    def test_class_linked(self, vmlinux):
        ret = get_json_entries(f"deps_for --path={vmlinux} --filter=[class=linked] --json -n=0", 100, 3000, is_normpath)
        for ent in ret["entries"]:
            assert is_linked(ent)

    def test_class_linked_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --filter=[class=linked]", 100, 3000, is_normpath)
        for ent in ret.split(os.linesep):
            assert is_linked(ent)

    def test_class_compiled(self, vmlinux):
        ret = get_json_entries(f"deps_for --path={vmlinux} --filter=[class=compiled] --json -n=0", 1000, 4000, is_normpath)
        for ent in ret["entries"]:
            assert is_compiled(ent)

    def test_class_compiled_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --filter=[class=compiled]", 1000, 4000, is_normpath)
        for ent in ret.split(os.linesep):
            assert is_compiled(ent)

    def test_direct(self, vmlinux):
        get_json_entries(f"deps_for --path={vmlinux} --direct --json", 1000, 10000, is_normpath)

    def test_direct_raw(self, vmlinux):
        get_raw(f"deps_for --path={vmlinux} --direct", 1000, 10000, is_normpath)

    def test_commands(self, vmlinux):
        get_json_entries(f"deps_for --path={vmlinux} --commands --json", 2000, 20000, is_command)

    def test_commands_raw(self, vmlinux):
        get_raw(f"deps_for --path={vmlinux} --commands", 2000, 20000, is_raw_command)

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
        ret = get_raw(f"deps_for --path={vmlinux} --rdm --filter=[path=.*\\.c$|.*\\.h$,type=re]")
        assert is_raw_rdm(ret)
        for ent in ret.split(os.linesep):
            assert ent.startswith("/") or ent.startswith("  /")
            if ent.startswith("/"):
                assert is_normpath(ent)
                assert ent.endswith(".c") or ent.endswith(".h")
            if ent.startswith("  /"):
                assert is_normpath(ent)

    def test_cdm(self, vmlinux):
        ret = get_rdm(f"deps_for --path={vmlinux} --filter=[class=linked] --cdm -n=0 --json", 100, 30000, is_cdm)
        for key in ret:
            assert is_linked(key)
            for ent in ret[key]:
                assert is_compiled(ent)

    def test_cdm_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --filter=[class=linked] --cdm -n=0")
        assert is_raw_rdm(ret)
        for ent in ret.split(os.linesep):
            assert ent.startswith("/") or ent.startswith("  /")
            if ent.startswith("/"):
                assert is_linked(ent)
            if ent.startswith("  /"):
                assert is_compiled(ent)

    def test_revdeps(self, vmlinux):
        get_json_entries(f"deps_for --path={vmlinux} --revdeps --filter=[path=.*\\.c|.*\\.h,type=re] -n=0 --json", 2000, 10000, is_normpath)

    def test_dep_graph(self, vmlinux):
        get_dep_graph(f"deps_for --path={vmlinux} --dep-graph -n=0 --json", 2000, 15000, is_dep_graph)

    def test_dep_graph_raw(self, vmlinux):
        ret = get_raw(f"deps_for --path={vmlinux} --dep-graph -n=0")
        assert is_raw_dep_graph(ret)

    def test_count(self,vmlinux):
        assert get_json_entries(f"deps_for --path={vmlinux} --json ")["num_entries"] == \
            get_json_simple(f"deps_for --path={vmlinux} --json -l")["count"]


@pytest.mark.timeout(DEF_TIMEOUT)
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
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[0] == compiled_file

    def test_commands(self, compiled_file):
        get_json_entries(f"faccess --path={compiled_file}  --commands --json", 1, 20, is_command)

    def test_commands_raw(self, compiled_file):
        get_raw(f"faccess --path={compiled_file} --commands", 1, 20, is_raw_command)

    def test_generate(self, compiled_file):
        get_json_simple(f"faccess --path={compiled_file} --commands --filter=[access=r] --generate --json", 1, 50000, is_generated)

    def test_generate_raw(self, compiled_file):
        get_raw(f"faccess --path={compiled_file} --commands --filter=[access=r] --generate", 1, 50000, is_raw_generated)

    def test_mode(self, compiled_file):
        ret = get_json_entries(f"faccess --path={compiled_file} --filter=[access=r] --json", 1, 20)
        for ent in ret["entries"]:
            assert ent["access"] == "r"

    def test_mode_raw(self, compiled_file):
        ret = get_raw(f"faccess --path={compiled_file} --filter=[access=r]", 1, 20, is_raw_process)
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[1] == "r"

    def test_count(self,compiled_file):
        assert get_json_entries(f"faccess --path={compiled_file} --json ")["num_entries"] == \
            get_json_simple(f"faccess --path={compiled_file} --json -l")["count"]

@pytest.mark.timeout(DEF_TIMEOUT)
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
        for ent in ret.split(os.linesep):
            assert ent.endswith("vmlinux")

    def test_commands(self):
        get_json_entries("linked_modules --filter=[path=*vmlinux,type=wc,exists=1]and[compressed,negate=1] --commands --json", 1, 1, is_command)

    def test_commands_raw(self):
        get_raw("linked_modules --filter=[path=*vmlinux,type=wc,exists=1]and[compressed,negate=1] --commands", 1, 1, is_raw_command)

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
        for ent in ret.split(os.linesep):
            assert ent.endswith(".a")

    def test_linked_shared(self):
        ret = get_json_entries("linked_modules --filter=[class=linked_shared] -n=0 --json", 2000, 15000, is_normpath)
        for ent in ret["entries"]:
            assert ent.endswith(".so") or ent.endswith(".so.dbg") or ent.endswith(".so.raw") or \
            ent.endswith(".btf") or ent.endswith("/linker")or ent.endswith("/linker64") or "tmp.XXX" in ent or "kallsyms" in ent or "vmlinux" in ent

    def test_linked_shared_raw(self):
        ret = get_raw("linked_modules --filter=[class=linked_shared]", 2000, 15000, is_normpath)
        for ent in ret.split(os.linesep):
            assert ent.endswith(".so") or ent.endswith(".so.dbg") or ent.endswith(".so.raw") or \
            ent.endswith(".btf") or ent.endswith("/linker")or ent.endswith("/linker64") or "tmp.XXX" in ent or "kallsyms" in ent or "vmlinux" in ent

    def test_linked_exe(self):
        ret = get_json_entries("linked_modules --filter=[class=linked_exe] -n=0 --json", 500, 3000, is_normpath)
        for ent in ret["entries"]:
            assert  "out/" in ent

    def test_linked_exe_raw(self):
        ret = get_raw("linked_modules --filter=[class=linked_exe]", 500, 3000, is_normpath)
        for ent in ret.split(os.linesep):
            assert  "out/" in ent

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
        for ent in ret.split(os.linesep):
            assert not ent.startswith(nfsdb.source_root)

    def test_details_relative(self):
        ret = get_json_entries("linked_modules --details -n=0 --json --relative", 2000, 15000, is_open_details)
        for ent in ret["entries"]:
            assert not ent["filename"].startswith(nfsdb.source_root)

    def test_details_relative_raw(self):
        ret = get_raw("linked_modules --details --relative", 2000, 15000, is_raw_open_details)
        for ent in ret.split(os.linesep):
            assert not ent.split(RAW_SEP)[0].startswith(nfsdb.source_root)

    def test_count(self):
        assert get_json_entries("linked_modules --json ")["num_entries"] == \
            get_json_simple("linked_modules --json -l")["count"]

@pytest.mark.timeout(DEF_TIMEOUT)
class TestModdepsFor:
    def test_required_param(self):
        get_error("moddeps_for", error_keyword="Missing required args 'path'")

    def test_plain(self, vmlinux):
        get_json_entries(f"moddeps_for --path={vmlinux} -n=0 --json", 400, 900, is_normpath)

    def test_plain_raw(self, vmlinux):
        get_raw(f"moddeps_for --path={vmlinux}", 400, 900, is_normpath)

    def test_relative(self, vmlinux):
        ret = get_json_entries(f"moddeps_for --path={vmlinux} -n=0 --json --relative", 400, 900, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_relative_raw(self, vmlinux):
        ret = get_raw(f"moddeps_for --path={vmlinux} --relative", 400, 900, is_normpath)
        for ent in ret.split(os.linesep):
            assert not ent.startswith(nfsdb.source_root)

    def test_plain_direct(self, vmlinux):
        get_json_entries(f"moddeps_for --path={vmlinux} -n=0 --direct --json", 20, 100, is_normpath)

    def test_plain_direct_raw(self, vmlinux):
        get_raw(f"moddeps_for --path={vmlinux} --direct", 20, 100, is_normpath)

    def test_filter(self, vmlinux):
        get_json_entries(f"moddeps_for --path={vmlinux} --filter=[path=*.o,type=wc] -n=0 --json", 10, 50, is_normpath)

    def test_filter_raw(self, vmlinux):
        get_raw(f"moddeps_for --path={vmlinux} --filter=[path=*.o,type=wc]", 10, 50, is_normpath)

    def test_commands(self, vmlinux):
        get_json_entries(f"moddeps_for --path={vmlinux} --commands -n=0 --json", 400, 900, is_command)

    def test_commands_raw(self, vmlinux):
        get_raw(f"moddeps_for --path={vmlinux} --commands", 400, 900, is_raw_command)

    def test_details(self, vmlinux):
        get_json_entries(f"moddeps_for --path={vmlinux} --details -n=0 --json", 400, 2000, is_open_details)

    def test_details_raw(self, vmlinux):
        get_raw(f"moddeps_for --path={vmlinux} --details", 400, 2000, is_raw_open_details)

    def test_details_relative(self, vmlinux):
        ret = get_json_entries(f"moddeps_for --path={vmlinux} --details -n=0 --json --relative", 400, 2000, is_open_details)
        for ent in ret["entries"]:
            assert not ent["filename"].startswith(nfsdb.source_root)

    def test_details_relative_raw(self, vmlinux):
        ret = get_raw(f"moddeps_for --path={vmlinux} --details --relative", 400, 2000, is_raw_open_details)
        for ent in ret.split(os.linesep):
            assert not ent.split(RAW_SEP)[0].startswith(nfsdb.source_root)

    def test_generate(self, vmlinux):
        get_json_simple(f"moddeps_for --path={vmlinux} --commands --generate -n=0 --json", 400, 900, is_generated)

    def test_generate_raw(self, vmlinux):
        get_raw(f"moddeps_for --path={vmlinux} --commands --generate", 400, 900, is_raw_generated)

    def test_count(self, vmlinux):
        assert get_json_entries(f"moddeps_for --path={vmlinux} --json ",400,900)["num_entries"] == \
            get_json_simple(f"moddeps_for --path={vmlinux} --json -l")["count"]

@pytest.mark.timeout(DEF_TIMEOUT)
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
        for ent in ret.split(os.linesep):
            assert int(ent.split(RAW_SEP)[2]) in pids

    def test_commands(self, pids):
        ret = get_json_entries(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --commands --json", 1, 2000)
        for ent in ret["entries"]:
            assert ent["pid"] in pids

    def test_commands_raw(self, pids):
        ret = get_raw(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --commands", 1, 2000, is_raw_command)
        for ent in ret.split(os.linesep):
            assert int(ent.split(RAW_SEP)[0]) in pids

    def test_relative(self, pids):
        ret = get_json_entries(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --json --relative", 1, 2000, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_relative_raw(self, pids):
        ret = get_raw(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --relative", 1, 2000, is_normpath)
        for ent in ret.split(os.linesep):
            assert not ent.startswith(nfsdb.source_root)

    def test_details_relative(self, pids):
        ret = get_json_entries(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --details --json --relative", 1, 2000, is_open_details)
        for ent in ret["entries"]:
            assert ent["ppid"] in pids and not ent["filename"].startswith(nfsdb.source_root)

    def test_details_raw_relative(self, pids):
        ret = get_raw(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --details --relative", 1, 2000, is_raw_open_details)
        for ent in ret.split(os.linesep):
            assert int(ent.split(RAW_SEP)[2]) in pids and not ent.split(RAW_SEP)[0].startswith(nfsdb.source_root)

    def test_count(self, pids):
        assert get_json_entries(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --json ")["num_entries"] == \
            get_json_simple(f"procref --pid={pids[0]} --pid={pids[1]} --pid={pids[2]} --json -l")["count"]

@pytest.mark.timeout(600)
# @pytest.mark.skip()
class TestRefFiles:
    def test_plain(self):
        get_json_entries("ref_files -n=1000 --json", 1000000, 6000000, is_normpath)

    def test_plain_raw(self):
        get_raw("ref_files", 1000000, 6000000, is_normpath)

    def test_with_filter(self):
        ret = get_json_entries("ref_files --filter=[path=*.c,type=wc,exists=1,access=r] -n=1000 --json", 10000, 50000, is_normpath)
        for ent in ret["entries"]:
            assert ent.endswith(".c")

    def test_with_filter_raw(self):
        ret = get_raw("ref_files --filter=[path=*.c,type=wc,exists=1,access=r]", 10000, 50000, is_normpath)
        for ent in ret.split(os.linesep):
            assert ent.endswith(".c")

    def test_details(self):
        ret = get_json_entries("ref_files --filter=[path=*.c,type=wc,exists=1,access=r] --details -n=1000 --json", 10000, 250000, is_open_details)
        for ent in ret["entries"]:
            assert ent["filename"].endswith(".c") and ent["access"] == "r"

    def test_details_raw(self):
        ret = get_raw("ref_files --filter=[path=*.c,type=wc,exists=1,access=r] --details", 10000, 250000, is_raw_open_details)
        for ent in ret.split(os.linesep):
            assert ent.split(RAW_SEP)[0].endswith(".c") and ent.split(RAW_SEP)[4] == "r"

    def test_compile_commands(self):
        get_json_entries("ref_files --filter=[path=*.c,type=wc,exists=1] --commands -n=1000 --json", 20000, 50000, is_command)

    def test_compile_commands_raw(self):
        get_raw("ref_files --filter=[path=*.c,type=wc,exists=1] --commands", 10000, 50000, is_raw_command)

    def test_rdm(self):
        get_rdm("ref_files --filter=[path=*.c,type=wc,exists=1] --rdm --json -n=0", 10000, 50000, is_rdm)

    def test_relative(self):
        ret = get_json_entries("ref_files --json --relative",1000000, 6000000, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_relative_raw(self):
        ret = get_raw("ref_files --relative", 1000000, 6000000, is_normpath)
        for ent in ret.split(os.linesep):
            assert not ent.startswith(nfsdb.source_root)

    def test_count(self):
        assert get_json_entries("ref_files --json ")["num_entries"] == \
            get_json_simple("ref_files --json -l")["count"]

    def test_filter_count_class(self, ref_files_len):

        linked_len = get_json_entries("ref_files --filter=[class=linked] --json -n=1")['num_entries']
        le_len = get_json_entries("ref_files --filter=[class=linked_exe] --json -n=1")['num_entries']
        lt_len = get_json_entries("ref_files --filter=[class=linked_static] --json -n=1")['num_entries']
        lh_len = get_json_entries("ref_files --filter=[class=linked_shared] --json -n=1")['num_entries']
        assert linked_len == (le_len + lt_len + lh_len)

        nl_len = get_json_entries("ref_files --filter=[class=linked,negate=1] --json -n=1")['num_entries']
        assert ref_files_len == (linked_len + nl_len)

        compiled_len = get_json_entries("ref_files --filter=[class=compiled] --json -n=1")['num_entries']
        n_comp_len = get_json_entries("ref_files --filter=[class=compiled,negate=1] --json -n=1")['num_entries']
        assert ref_files_len == (compiled_len + n_comp_len)

        plain_len = get_json_entries("ref_files --filter=[class=plain] --json -n=1")['num_entries']
        n_plain_len = get_json_entries("ref_files --filter=[class=plain,negate=1] --json -n=1")['num_entries']
        assert ref_files_len == (plain_len + n_plain_len)

        compiler_len = get_json_entries("ref_files --filter=[class=compiler] --json -n=1")['num_entries']
        n_compiler_len = get_json_entries("ref_files --filter=[class=compiler,negate=1] --json -n=1")['num_entries']
        assert compiler_len != 0 , "Case not supported yet"
        assert n_comp_len != 0
        assert ref_files_len == (compiler_len + n_compiler_len)

        linker_len = get_json_entries("ref_files --filter=[class=linker] --json -n=1")['num_entries']
        n_linker_len = get_json_entries("ref_files --filter=[class=linker,negate=1] --json -n=1")['num_entries']
        assert linker_len != 0 , "Case not supported yet"
        assert n_linker_len != 0
        assert ref_files_len == (linker_len + n_linker_len)

        assert ref_files_len == (linked_len + compiled_len + plain_len + compiler_len + linker_len)

    def test_filter_count_exist(self, ref_files_len):
        fil_ex_len = get_json_entries("ref_files --filter=[exists=1] --json -n=1")['num_entries']
        dir_ex_len = get_json_entries("ref_files --filter=[exists=2] --json -n=1")['num_entries']
        n_ex_len = get_json_entries("ref_files --filter=[exists=0] --json -n=1")['num_entries']
        assert ref_files_len == (fil_ex_len + dir_ex_len + n_ex_len)

    def test_filter_count_source_root(self, ref_files_len):
        sr_len = get_json_entries("ref_files --filter=[source_root=1] --json -n=1")['num_entries']
        nsr_len = get_json_entries("ref_files --filter=[source_root=0] --json -n=1")['num_entries']
        assert ref_files_len == (sr_len + nsr_len)

    def test_filter_count_symlink(self):
        fil_ex_len = get_json_entries("ref_files --filter=[exists=1] --json -n=1")['num_entries']
        sl_len = get_json_entries("ref_files --filter=[exists=1,link=1] --json -n=1")['num_entries']
        nsl_len = get_json_entries("ref_files --filter=[exists=1,link=0] --json -n=1")['num_entries']
        assert fil_ex_len == (sl_len + nsl_len)

    def test_filter_count_access(self, ref_files_len):
        r_len = get_json_entries("ref_files --filter=[access=r] --json -n=1")['num_entries']
        w_len = get_json_entries("ref_files --filter=[access=w] --json -n=1")['num_entries']
        rw_len = get_json_entries("ref_files --filter=[access=rw] --json -n=1")['num_entries']
        assert ref_files_len == (r_len + w_len + rw_len)

@pytest.mark.timeout(DEF_TIMEOUT)
class TestRevCompsFor:
    def test_plain(self, header_file):
        ret = get_json_entries(f"revcomps_for --path={header_file} --json", 1 , 200 , is_normpath)
        for ent in ret["entries"]:
            assert is_compiled(ent)

    def test_plain_raw(self, header_file):
        ret = get_raw(f"revcomps_for --path={header_file}", 1 , 200 , is_normpath)
        for ent in ret.split(os.linesep):
            assert is_compiled(ent)

    def test_filter(self, header_file):
        ret = get_json_entries(f"revcomps_for --path={header_file} --filter=[path=*.cc,type=wc] --json", 1, 200, is_normpath)
        for ent in ret["entries"]:
            assert ent.endswith(".cc")

    def test_filter_raw(self, header_file):
        ret = get_raw(f"revcomps_for --path={header_file} --filter=[path=*.cc,type=wc]", 1, 200, is_normpath)
        for ent in ret.split(os.linesep):
            assert ent.endswith(".cc")

    def test_relative(self, header_file):
        ret = get_json_entries(f"revcomps_for --path={header_file} --filter=[path=*.c,type=wc]or[path=*.cc,type=wc] --relative --json", 1, 200, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_relative_raw(self, header_file):
        ret = get_raw(f"revcomps_for --path={header_file} --filter=[path=*.c,type=wc]or[path=*.cc,type=wc] --relative", 1, 200, is_normpath)
        for ent in ret.split(os.linesep):
            assert not ent.startswith(nfsdb.source_root)

    def test_count(self, header_file):
        assert get_json_entries(f"revcomps_for --path={header_file} --json ")["num_entries"] == \
            get_json_simple(f"revcomps_for --path={header_file} --json -l")["count"]

@pytest.mark.timeout(DEF_TIMEOUT)
class TestRevDepsFor:
    def test_plain(self, compiled_file):
        get_json_entries(f"revdeps_for --path={compiled_file} --json", 1, 1000 ,is_normpath)

    def test_plain_raw(self, compiled_file,):
        get_raw(f"revdeps_for --path={compiled_file}", 1 , 1000, is_normpath)

    def test_plain_multiple(self, compiled_file, header_file):
        get_json_entries(f"revdeps_for --path={compiled_file} --path={header_file} --json", 1, 1000 ,is_normpath)

    def test_plain_multiple_raw(self, compiled_file, header_file):
        get_raw(f"revdeps_for --path={compiled_file} --path={header_file}", 1 , 1000, is_normpath)

    def test_recursive(self, compiled_file):
        r_normal = get_json_entries(f"revdeps_for --path={compiled_file} --json", 1, 1000, is_normpath)
        r_recursive = get_json_entries(f"revdeps_for --path={compiled_file} --recursive --json", 1, 1000, is_normpath)
        assert r_normal["count"] < r_recursive["count"]

    def test_recursive_raw(self, compiled_file):
        r_normal = get_raw(f"revdeps_for --path={compiled_file}", 1, 1000, is_normpath)
        r_recursive = get_raw(f"revdeps_for --path={compiled_file} --recursive", 1, 1000, is_normpath)
        assert len(r_normal.split(os.linesep)) < len(r_recursive.split(os.linesep))

    def test_filter(self, compiled_file):
        get_json_entries(f"revdeps_for --path={compiled_file} --filter=[path=*curl*,type=wc] --json", 1, 20, is_normpath)

    def test_filter_raw(self, compiled_file):
        get_raw(f"revdeps_for --path={compiled_file} --filter=[path=*curl*,type=wc]", 1, 20, is_normpath)

    def test_relative(self, compiled_file):
        ret = get_json_entries(f"revdeps_for --path={compiled_file} --filter=[path=*curl*,type=wc] --json --relative", 1, 20, is_normpath)
        for ent in ret["entries"]:
            assert not ent.startswith(nfsdb.source_root)

    def test_relative_raw(self, compiled_file):
        ret = get_raw(f"revdeps_for --path={compiled_file} --filter=[path=*curl*,type=wc] --relative", 1, 20, is_normpath)
        for ent in ret.split(os.linesep):
            assert not ent.startswith(nfsdb.source_root)

    def test_cdm(self, compiled_file):
        ret = get_rdm(f"revdeps_for --path={compiled_file} --json --cdm -n=0", 1, 1000, is_cdm)
        for key in ret:
            assert is_linked(key)
            for ent in ret[key]:
                assert is_compiled(ent)

    def test_cdm_raw(self, compiled_file):
        ret = get_raw(f"revdeps_for --path={compiled_file} --cdm", 1, 10000)
        assert is_raw_rdm(ret)
        for ent in ret.split(os.linesep):
            if ent.startswith("/"):
                assert is_linked(ent)
            if ent.startswith("  /"):
                assert is_compiled(ent)

    def test_count(self, compiled_file):
        assert get_json_entries(f"revdeps_for --path={compiled_file} --json ")["num_entries"] == \
            get_json_simple(f"revdeps_for --path={compiled_file} --json -l")["count"]

@pytest.mark.timeout(DEF_TIMEOUT)
class TestPipeline:
    def test_binaries_2_commands(self):
        ret = get_json_entries("binaries --filter=[bin=*javac,type=wc] commands --json", 300, 1500, is_command)
        for ent in ret["entries"]:
            assert fnmatch.fnmatch(ent['bin'], "*javac")
        count = get_json_simple("binaries -l --filter=[bin=*javac,type=wc] commands --json")["count"]
        assert ret["num_entries"] == count

    def test_linked_modules_2_deps_for(self):
        ret = get_json_entries("linked_modules --filter=[path=*vmlinux,type=wc] deps_for --json", 13000, 25000, is_normpath)
        count = get_json_simple("linked_modules -l --filter=[path=*vmlinux,type=wc] deps_for --json")["count"]
        assert ret["num_entries"] == count

class TestMiscModules:
    def test_version(self):
        ret = get_json_simple("version --json")
        assert ret["version"] == nfsdb.get_version()

    def test_version_raw(self):
        ret = get_raw("version")
        assert ret == nfsdb.get_version()

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
        assert ret == nfsdb.source_root

    def test_show_config(self):
        get_json_simple("config --json")

    def test_show_config_raw(self):
        get_raw("config")

    def test_show_stats(self):
        get_json_simple("stat --json")

    def test_stats_raw(self):
        get_raw("stat")
