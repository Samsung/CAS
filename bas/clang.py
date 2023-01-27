import os
import sys
import string
import subprocess
import libetrace

COMPILER_NONE = 0
COMPILER_C = 1
COMPILER_CPP = 2
COMPILER_OTHER = 3

test_file = "/tmp/.nfsdb__test.c"
if not os.path.exists(test_file):
    with open(test_file, "w", encoding=sys.getfilesystemencoding()) as out_f:
        out_f.write(";\n")

def get_wr_files(pid, fork_map, wr_map):

    wrs = set()
    if pid in wr_map:
        wrs = wrs | set(wr_map[pid])
    if pid in fork_map:
        childs = fork_map[pid]
        for child in childs:
            wrs = wrs | get_wr_files(child,fork_map,wr_map)
    return wrs

def process_write_open_files_unique_with_children(pid, fork_map, wr_map):
    return get_wr_files(pid, fork_map, wr_map)

def parse_config(out):
    intro="#include \"...\" search starts here:"
    middle="#include <...> search starts here:"
    outro="End of search list."
    lns = [x.strip() for x in out.split("\n") if x.strip()!=""]
    includes = []
    try:
        i=lns.index(intro)
        m=lns.index(middle, i)
        o=lns.index(outro, m)
        includes = lns[i+1:m]+lns[m+1:o]
    except ValueError:
        pass
    return includes

def detect_integrated_clang_compilers(compiler_list):
    return [comp for comp in compiler_list if  is_integrated_clang_compiler(comp)]

def is_integrated_clang_compiler(compiler_path):
    pn = subprocess.Popen(["%s" % compiler_path, "-c", "-Wall", "-o", "/dev/null", test_file, "-###"],\
        shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out, _ = pn.communicate()
    return " (in-process)" in out.decode("utf-8")


class clang(libetrace.clang):

    def __init__(self,verbose,debug,clang_compilers,clangpp_compilers,integrated_clang_compilers,debug_compilations=False,allow_pp_in_compilations=False):
        super(clang, self).__init__(verbose,debug,debug_compilations)
        self.c_compilers = clang_compilers
        self.c_preprocessors = clang_compilers
        self.c_include_paths = list()
        self.cc_compilers = clangpp_compilers
        self.cc_preprocessors = clangpp_compilers
        self.cc_include_paths = list()
        self.integrated_clang_compilers = integrated_clang_compilers
        self.allow_pp_in_compilations = allow_pp_in_compilations
        for cp in self.c_preprocessors:
            pn = subprocess.Popen([cp,"-E","-x","c","-","-v"],shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            out, err = pn.communicate()
            c_include_paths = parse_config(out.decode("utf-8"))
            self.c_include_paths.append(c_include_paths)
        for ccp in self.cc_preprocessors:
            pn = subprocess.Popen([ccp,"-E","-x","c++","-","-v"],shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            out,err = pn.communicate()
            cc_include_paths = parse_config(out.decode("utf-8"))
            self.cc_include_paths.append(cc_include_paths)
        if debug:
            for cpi_tuple in zip(self.c_compilers,self.c_preprocessors,self.c_include_paths):
                print ("[%s] [%s] %s\n"%(cpi_tuple[0],cpi_tuple[1],cpi_tuple[2]))
            for ccpi_tuple in zip(self.cc_compilers,self.cc_preprocessors,self.cc_include_paths):
                print ("[%s] [%s] %s\n"%(ccpi_tuple[0],ccpi_tuple[1],ccpi_tuple[2]))

    def compiler_type(self, cbin):
        if cbin in self.c_compilers:
            i = self.c_compilers.index(cbin)
            return COMPILER_C
        elif cbin in self.cc_compilers:
            i = self.cc_compilers.index(cbin)
            return COMPILER_CPP
        else:
            return None

    def compiler_include_paths(self,cbin):
        if cbin in self.c_compilers:
            i = self.c_compilers.index(cbin)
            return self.c_include_paths[i]
        elif cbin in self.cc_compilers:
            i = self.cc_compilers.index(cbin)
            return self.cc_include_paths[i]
        else:
            return []

    def parse_include_files(self, exe, ipaths):
        cmd = exe['v']
        ifiles = list()
        include_opt_indices = [i for i, x in enumerate(cmd) if x == "-include"]
        for i in include_opt_indices:
            if os.path.isabs(cmd[i+1]):
                ifiles.append(cmd[i+1])
            else:
                tryPaths = [os.path.normpath(os.path.join(x,cmd[i+1])) for x in ipaths]+[os.path.realpath(os.path.normpath(os.path.join(exe['w'],cmd[i+1])))]
                pathsExist = [x for x in tryPaths if os.path.isfile(x)]
                if len(pathsExist) > 0:
                    ifiles.append(pathsExist[0])
        return ifiles

    # def get_compiled_files(self,out,exe):
    #     return out[0]

    def have_integrated_cc1(self, exe):
        return "-fno-integrated-cc1" not in exe[2] and \
                os.path.normpath(os.path.join(exe[1],exe[0])) in self.integrated_clang_compilers

    def get_object_files(self, comp_exe, have_int_cc1, fork_map, rev_fork_map, wr_map):
        if have_int_cc1:
            cpid = comp_exe["p"]
        else:
            cpid = rev_fork_map[comp_exe["p"]]

        return [
            x for x in process_write_open_files_unique_with_children(cpid,fork_map,wr_map)
            if not x.startswith("/dev/") and libetrace.is_ELF_or_LLVM_BC_file(x)
            ]

    @staticmethod
    def parse_one_def(s,st):
        i = next((i for i, ch  in enumerate(s) if ch in st),None)
        if i:
            return (s[:i],s[i:].strip())
        else:
            return (s,"")

    def parse_defs(self, out):
        intro="#include \"...\" search starts here:"
        middle="#include <...> search starts here:"
        outro="End of search list."
        lns = [x.strip() for x in out.split("\n") if x.strip()!=""]

        st = set(string.whitespace)

        includes = list()
        try:
            i=lns.index(intro)
            o=lns.index(outro, i)
            if middle in lns:
                m=lns.index(middle)
                includes += lns[i+1:m]+lns[m+1:o]
            else:
                includes += lns[i+1:o]

        except ValueError as err:
            print ("@exception(%s)"%(str(err)), flush=True)
            print ("@data(%s)"%(lns), flush=True)
            raise err

        defs = [self.parse_one_def(x[7:].lstrip(),st) for x in lns[o+1:] if x.startswith("#define")]
        undefs = [self.parse_one_def(x[6:].lstrip(),st) for x in lns[o+1:] if x.startswith("#undef")]

        return (includes,defs,undefs)

    def get_source_type(self,cmd,compiler_type,src_ext):

        comp_lan_indices = [i for i, x in enumerate(cmd) if x == "-x"]
        """
        So far only first specification of -x (if any) is applied to all source files; proper parsing of command line is required to handle that well
        """
        cmd_lan_spec = cmd[comp_lan_indices[0]+1] if len(comp_lan_indices)>0 else None
            
        if cmd_lan_spec:
            if cmd_lan_spec in ["c","c-header","cpp-output"]:
                return COMPILER_C
            elif cmd_lan_spec in ["c++","c++-header","c++-cpp-output"]:
                return COMPILER_CPP
            elif cmd_lan_spec in ["assembler-with-cpp"]:
                return COMPILER_OTHER
        if compiler_type==COMPILER_CPP:
            return COMPILER_CPP
        else:
            if src_ext==".c":
                return COMPILER_C
            else:
                return COMPILER_CPP
