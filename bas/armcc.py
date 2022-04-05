import sys
sys.dont_write_bytecode = True

import os
import re
import subprocess
import libetrace
import string

COMPILER_NONE = 0
COMPILER_C = 1
COMPILER_CPP = 2

class armcc(libetrace.armcc):

    def __init__(self,verbose,debug,debug_compilations=False):
        super(armcc, self).__init__(verbose,debug,debug_compilations)

    def compiler_type(self,cbin):
        return COMPILER_NONE

    def compiler_include_paths(self,cbin):
        return None

    def parse_include_files(self,exe,ipaths):
        cmd = exe[2]
        ifiles = list()
        include_opt_indices = [i for i, x in enumerate(cmd) if x.startswith("--preinclude")]
        for i in include_opt_indices:
            if "=" in cmd[i]:
                val = "=".join(cmd[i].split("=")[1:])
            else:
                val = cmd[i+1]
            if os.path.isabs(val):
                ifiles.append(val)
            else:
                tryPaths = [os.path.normpath(os.path.join(x,cmd[i+1])) for x in ipaths]+[os.path.realpath(os.path.normpath(os.path.join(exe[1],cmd[i+1])))]
                pathsExist = [x for x in tryPaths if os.path.isfile(x)]
                if (len(pathsExist)>0):
                    ifiles.append(pathsExist[0])
        return ifiles

    def get_compiled_files(self,out,exe):
        return [ flst.split("\n")[0].split()[2].strip("\"") for flst in out[0] ]

    def get_object_files(self,comp_exe,fork_map,rev_fork_map,wr_map):
        return []

    def parse_defs(self,out,exe):

        def path_to_include(include_line):
            if len(include_line.strip())<=0 or include_line[0]!="N":
                return None
            include_line = (" " + include_line[1:]).strip()[1:].strip().split()
            if include_line[0]!="include":
                return None
            iline = include_line[1]
            if "<" in iline and ">" in iline or "\"" in iline:
                return include_line[1].strip("<").strip(">").strip("\"")
            else:
                return None

        idirs = set()
        cwd = exe[1]
        for outv in out[0]:
            prevline = ""
            for line in outv.split("\n")[1:]:
                if line.startswith("L") and int(line.split()[-1])==1:
                    iline = line.split()[-2].strip("\"")
                    ipath = path_to_include(prevline)
                    if ipath:
                        ipath = os.path.normpath(ipath)
                        if not iline.startswith("/"):
                            iline = os.path.join(cwd,iline)
                        ilines = iline.split("/")
                        for u in reversed(ipath.split("/")):
                            if u==".":
                                pass
                            elif u=="..":
                                start_dir = "/".join(ilines)
                                subdirs = [x for x in os.listdir(start_dir) if os.path.isdir(os.path.join(start_dir,x))]
                                if len(subdirs)>0:
                                    ilines.append(subdirs[0])
                            else:
                                if u!=ilines[-1]:
                                    print ("include (%s) not at (%s)"%(u,iline))
                                ilines = ilines[:-1]
                            idirs.add("/".join(ilines))
                    else:
                        ils = iline.split("/")[:-1]
                        if len(ils)>0:
                            if ils[0]=="":
                                idirs.add("/".join(ils))
                            else:
                                idirs.add(os.path.join(cwd,"/".join(ils)))
                        else:
                            idirs.add(cwd)
                prevline = line
        includes = list(idirs)

        def parse_one_def(s):
            st = set(string.whitespace)
            i = next((i for i, ch  in enumerate(s) if ch in st),None)
            if i:
                return (s[:i],s[i:].strip())
            else:
                return (s,"")

        defs = [parse_one_def(x[7:].lstrip()) for x in out[1].split("\n") if x.startswith("#define")]
        undefs = [parse_one_def(x[6:].lstrip()) for x in out[1].split("\n") if x.startswith("#undef")]
        return (includes,defs,undefs)

    def get_source_type(self,cmd,compiler_type,src_ext):

        if "--cpp" in cmd or "--cpp11" in cmd:
            return COMPILER_CPP
        if "--c90" in cmd or "--c99" in cmd:
            return COMPILER_C

        if compiler_type==COMPILER_CPP:
            return COMPILER_CPP
        else:
            if src_ext in [".c",".ac",".tc"]:
                return COMPILER_C
            else:
                return COMPILER_CPP
