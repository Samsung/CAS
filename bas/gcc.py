import sys
sys.dont_write_bytecode = True

import os
import re
import subprocess
import libetrace
import string
import fnmatch

COMPILER_NONE = 0
COMPILER_C = 1
COMPILER_CPP = 2
COMPILER_OTHER = 3

def get_wr_files(pid,fork_map,wr_map):

    wrs = set()
    if pid in wr_map:
        wrs = wrs | set(wr_map[pid])
    if pid in fork_map:
        childs = fork_map[pid]
        for child in childs:
            wrs = wrs | get_wr_files(child,fork_map,wr_map)
    return wrs

def process_write_open_files_unique_with_children(pid,fork_map,wr_map):

    return get_wr_files(pid,fork_map,wr_map)

def parse_config(out):
    intro="#include \"...\" search starts here:"
    middle="#include <...> search starts here:"
    outro="End of search list."
    lns = [x.strip() for x in out.split("\n") if x.strip()!=""]
    includes = list()
    try:
        i=lns.index(intro)
        m=lns.index(middle)
        o=lns.index(outro)
        includes += lns[i+1:m]+lns[m+1:o]
    except Exception:
        pass
    return includes

class gcc(libetrace.gcc):

    def __init__(self,verbose,debug,gcc_compilers,gpp_compilers,debug_compilations=False):
        super(gcc, self).__init__(verbose,debug,debug_compilations)
        self.c_compilers = gcc_compilers
        self.c_preprocessors = list()
        self.c_include_paths = list()
        self.cc_compilers = gpp_compilers
        self.cc_preprocessors = list()
        self.cc_include_paths = list()
        for c in self.c_compilers:
            pn = subprocess.Popen([c,"-print-prog-name=cc1"],shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            out,err = pn.communicate("")
            cc1 = out.decode("utf-8").strip()
            if not os.path.isabs(cc1):
                cc1 = os.path.join(os.path.dirname(c),cc1)
            self.c_preprocessors.append(cc1)
        for cc in self.cc_compilers:
            pn = subprocess.Popen([cc,"-print-prog-name=cc1plus"],shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            out,err = pn.communicate("")
            cc1plus = out.decode("utf-8").strip()
            if not os.path.isabs(cc1plus):
                cc1plus = os.path.join(os.path.dirname(cc),cc1plus)
            self.cc_preprocessors.append(cc1plus)
        for cp in self.c_preprocessors:
            pn = subprocess.Popen([cp,"-v"],shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            out,err = pn.communicate("")
            c_include_paths = parse_config(out.decode("utf-8"))
            self.c_include_paths.append(c_include_paths)
        for ccp in self.cc_preprocessors:
            pn = subprocess.Popen([ccp,"-v"],shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            out,err = pn.communicate("")
            cc_include_paths = parse_config(out.decode("utf-8"))
            self.cc_include_paths.append(cc_include_paths)
        if debug:
            for cpi_tuple in zip(self.c_compilers,self.c_preprocessors,self.c_include_paths):
                print ("[%s] [%s] %s\n"%(cpi_tuple[0],cpi_tuple[1],cpi_tuple[2]))
            for ccpi_tuple in zip(self.cc_compilers,self.cc_preprocessors,self.cc_include_paths):
                print ("[%s] [%s] %s\n"%(ccpi_tuple[0],ccpi_tuple[1],ccpi_tuple[2]))

    def compiler_type(self,cbin):
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
            return None

    def parse_include_files(self,exe,ipaths):
        cmd = exe[2]
        ifiles = list()
        include_opt_indices = [i for i, x in enumerate(cmd) if x == "-include"]
        for i in include_opt_indices:
            if os.path.isabs(cmd[i+1]):
                ifiles.append(cmd[i+1])
            else:
                tryPaths = [os.path.normpath(os.path.join(x,cmd[i+1])) for x in ipaths]+[os.path.realpath(os.path.normpath(os.path.join(exe[1],cmd[i+1])))]
                pathsExist = [x for x in tryPaths if os.path.isfile(x)]
                if (len(pathsExist)>0):
                    ifiles.append(pathsExist[0])
        return ifiles

    def get_compiled_files(self,out,exe):
        return out[0]

    def get_object_files(self,comp_exe,fork_map,rev_fork_map,wr_map):
        return [x for x in process_write_open_files_unique_with_children(comp_exe["p"],fork_map,wr_map) if not fnmatch.fnmatch(x,"/dev/*") and libetrace.is_ELF_file(x)]

    def parse_defs(self,out,exe):
        intro="#include \"...\" search starts here:"
        middle="#include <...> search starts here:"
        outro="End of search list."
        lns = [x.strip() for x in out[1].split("\n") if x.strip()!=""]

        def parse_one_def(s):
            st = set(string.whitespace)
            i = next((i for i, ch  in enumerate(s) if ch in st),None)
            if i:
                return (s[:i],s[i:].strip())
            else:
                return (s,"")

        includes = list()
        try:
            i=lns.index(intro)
            m=lns.index(middle)
            o=lns.index(outro)
            includes += lns[i+1:m]+lns[m+1:o]
        except Exception:
            pass

        defs = [parse_one_def(x[7:].lstrip()) for x in lns[o+1:] if x.startswith("#define")]
        undefs = [parse_one_def(x[6:].lstrip()) for x in lns[o+1:] if x.startswith("#undef")]

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
