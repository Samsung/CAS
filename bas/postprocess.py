#!/usr/bin/env python3

import os
import sys
import json
import argparse
import multiprocessing
import time
import base64
import json
import fnmatch
import subprocess
import pickle as cPickle
import clang
import gcc
import math
import functools
import copy

parser = argparse.ArgumentParser(description="MAIN_DESCRIPTION", formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--create-nfsdb-cache", action="store_true", help="")
parser.add_argument("--debug", action="store_true", help="")
parser.add_argument("--verbose", action="store_true", help="")
parser.add_argument("--debug-compilations", action="store_true", help="")
parser.add_argument("--allow-pp-in-compilations", action="store_true", help="")
parser.add_argument("--config", action="store", help="")
parser.add_argument("--dbversion", action="store", help="Database version string")
parser.add_argument("--integrated-clang-compilers", action="store", help="")
parser.add_argument("--exclude-command-patterns", action="store", help="Provide list of patterns to precompute matching with all commands (delimited by ':')")
parser.add_argument("-j", "--jobs", action="store", help="Specify number of jobs for the compilation processing [ default: cpu_count() ]")
parser.add_argument('db_path', action="store")
args = parser.parse_args()

if not args.jobs:
    args.jobs = multiprocessing.cpu_count()
else:
    args.jobs = int(args.jobs)

if not args.dbversion:
    dbversion = ""
else:
    dbversion = args.dbversion

if not args.integrated_clang_compilers:
    args.integrated_clang_compilers = []
else:
    args.integrated_clang_compilers = args.integrated_clang_compilers.split(":")

if not args.config:
    args.config = os.path.dirname(os.path.realpath(__file__))

if args.exclude_command_patterns is None:
    args.exclude_command_patterns = []
else:
    args.exclude_command_patterns = args.exclude_command_patterns.split(":")

with open(".nfsdb","r") as f:
    source_root = f.readline().strip().split("=")[1]

with open(args.db_path,"rb") as f:
    nfsdb = json.loads(f.read())

start_time = time.time()
# Check if database entries have unique keys
nnfsdb = []
kset = set()
for e in nfsdb:
    k = (e["p"],e["x"])
    if k in kset:
        print ("WARNING: Duplicated entry with key: [%d:%d]"%(e["p"],e["x"]))
    else:
        kset.add(k)
        nnfsdb.append(e)
print ("--- check for unique keys passed  [%.2fs]"%(time.time()-start_time))
nfsdb = nnfsdb

if args.create_nfsdb_cache:
    import libetrace
    libetrace.create_nfsdb(nfsdb,source_root,dbversion,None,args.exclude_command_patterns,"%s.img"%(args.db_path))
    sys.exit(0)

bins = set(os.path.join(e["w"],e["b"]) for e in nfsdb)
start_time = time.time()
pm = {} # { PID: [execs0, execs1, ...] }
bex = {} # { BIN: [execs0, execs1, ...] }
for e in nfsdb:
    b = os.path.join(e["w"],e["b"])
    if b in bex:
        bex[b].append(e)
    else:
        bex[b] = [e]
    pid = e["p"]
    if pid in pm:
        pm[pid].append(e)
    else:
        pm[pid] = [e]
print ("--- process map / binary map created  [%.2fs]"%(time.time()-start_time))

start_time = time.time()
fork_map = {}
for e in nfsdb:
    if e["p"] in fork_map:
        fork_map[e["p"]]+=[x["p"] for x in e["c"]]
    else:
        fork_map[e["p"]] = [x["p"] for x in e["c"]]
rev_fork_map = {}
for pid in fork_map:
    childs = fork_map[pid]
    for child in childs:
        if child in rev_fork_map:
            assert True, "Multiple parents for process %d"%(child)
        else:
            rev_fork_map[child] = pid

print ("--- fork maps created  [%.2fs]"%(time.time()-start_time))

start_time = time.time()
# pipe_map {pid:{pid0,pid1,...}}
pipe_map = {}
for e in nfsdb:
    pids = set([x["p"] for x in e["i"]])
    if e["p"] in pipe_map:
        pipe_map[e["p"]]|=pids
    else:
        pipe_map[e["p"]] = pids
print ("--- pipe_map extracted  [%.2fs]"%(time.time()-start_time))

start_time = time.time()
wr_map = {}
for e in nfsdb:
    fns = [x["p"] for x in e["o"] if x["m"]&3>=1]+[os.path.normpath(x["o"]) for x in e["o"] if "o" in x and x["m"]&3>=1]
    if e["p"] in wr_map:
        wr_map[e["p"]]+=list(set(wr_map[e["p"]]+fns))
    else:
        wr_map[e["p"]] = list(set(fns))

print ("--- file write map created  [%.2fs]"%(time.time()-start_time))

def set_children_list(pid,ptree,ignore_binaries):

    if pid in fork_map:
        for chld in fork_map[pid]:
            if not ignore_binaries or not set([u["b"] for u in pm[pid]])&ignore_binaries:
                set_children_list(chld,ptree,ignore_binaries)

    assert not pid in ptree, "process tree have entry for pid %d"%(pid)
    if pid in fork_map:
        ptree[pid] = fork_map[pid]
    else:
        ptree[pid] = list()

def process_tree_for_pid(pid,ignore_binaries):

    ptree = {}
    set_children_list(pid,ptree,ignore_binaries)    
    return ptree

def update_reverse_binary_mapping(reverse_binary_mapping,pid,ptree,root_pid,root_binary):
    if pid in ptree:
        for chld_pid in ptree[pid]:
            if chld_pid in reverse_binary_mapping:
                assert reverse_binary_mapping[chld_pid]==pid,"reverse binary mapping contains: %d (root: %d)"%(chld_pid,root_pid)
            else:
                exelst = pm[chld_pid] if chld_pid in pm else []
                if root_binary not in set([u["b"] for u in exelst]):
                    reverse_binary_mapping[chld_pid] = root_pid
                update_reverse_binary_mapping(reverse_binary_mapping,chld_pid,ptree,root_pid,root_binary)

def build_reverse_binary_mapping(binlst):
    reverse_binary_mapping = dict()
    for binary in binlst:
        if binary in bex:
            for e in bex[binary]:
                ptree = process_tree_for_pid(e["p"],set(binlst))
                if ptree is not None:
                    update_reverse_binary_mapping(reverse_binary_mapping,e["p"],ptree,e["p"],binary)
    return reverse_binary_mapping

with open(os.path.join(args.config,".bas_config"),"r") as f:
    config = json.loads(f.read())

integrated_clang_compilers = []
if args.integrated_clang_compilers:
    integrated_clang_compilers=list(set(integrated_clang_compilers+args.integrated_clang_compilers))
if "integrated_clang_compilers" in config:
    integrated_clang_compilers=list(set(integrated_clang_compilers+config["integrated_clang_compilers"]))

start_time = time.time()
rbm = build_reverse_binary_mapping(config["rbm_wrapping_binaries"])
for pid in rbm:
    for e in pm[pid]:
        e["m"] = rbm[pid]
print ("--- reverse binary mappings created (%d binaries)  [%.2fs]"%(len(["/bin/bash","/bin/sh"]),time.time()-start_time))

import libetrace
start_time = time.time()
#os.system('setterm -cursor off')
# { (upid,exeidx) : [pattern_match_byte_array] }
pcp_map = libetrace.precompute_command_patterns(args.exclude_command_patterns,pm,debug=args.debug)
#os.system('setterm -cursor on')
if pcp_map:
    for k,v in pcp_map.items():
        pid = k[0]
        exeidx = k[1]
        e = pm[pid][exeidx]
        e["n"] = base64.b64encode(v)
print ("--- command patterns precomputed (%d patterns)  [%.2fs]"%(len(args.exclude_command_patterns),time.time()-start_time))

start_time = time.time()
lnklst = [x for x in bins if any((u for u in config["ld_spec"] if fnmatch.fnmatch(x,u)))]
linked = set()
for lnk in lnklst:
    elst = bex[lnk]
    for e in elst:
        if "-o" in e["v"]:
            outidx = e["v"].index("-o")
            if outidx<len(e["v"])-1:
                lnkm = os.path.normpath(os.path.join(e["w"],e["v"][outidx+1]))
                if os.path.splitext(lnkm)[1]==".o" and outidx<len(e["v"])-2:
                    if os.path.basename(e["v"][outidx+2])==".tmp_%s"%(os.path.basename(lnkm)):
                        continue
                linked.add( (e["p"],e["x"],lnkm) )
        elif "--output" in e["v"]:
            outidx = e["v"].index("--output")
            if outidx<len(e["v"])-1:
                linked.add( (e["p"],e["x"],os.path.normpath(os.path.join(e["w"],e["v"][outidx+1]))) )
arlst = [x for x in bins if any((u for u in config["ar_spec"] if fnmatch.fnmatch(x,u)))]
ared = set()
for arb in arlst:
    alst = bex[arb]
    for e in alst:
        # We definitely needs better routine for grepping ar command line for archive filename
        armod = next((x for x in e["v"] if x.endswith(".a") or x.endswith(".a.tmp") or x.endswith(".lib") or x.endswith("built-in.o")),None)
        if armod:
            if armod.endswith(".a.tmp"):
                # There are some places in Android build system where ar creates '.a.tmp' archive file only to mv it to '.a' archive right away
                # Work around that by setting the linking process as the parent bash invocation (which does the ar and mv altogether)
                ared.add( (e["r"]["p"],e["r"]["x"],os.path.normpath(os.path.join(e["w"],armod[:-4]))) )
            else:
                ared.add( (e["p"],e["x"],os.path.normpath(os.path.join(e["w"],armod))) )

for m in linked:
    e = pm[m[0]][m[1]]
    e["l"] = m[2]
    e["t"] = 1
for m in ared:
    e = pm[m[0]][m[1]]
    e["l"] = m[2]
    e["t"] = 0
print ("--- computed linked modules (linked: %d, ared: %d)  [%.2fs]"%(len(linked),len(ared),time.time()-start_time))

compilation_start_time = time.time()
start_time = time.time()
allow_llvm_bc=True

def maybe_compiler_binary(binary_path_or_link):
    if binary_path_or_link.endswith("/cc") or binary_path_or_link.endswith("/c++"):
        return os.path.realpath(binary_path_or_link)
    else:
        return binary_path_or_link

gcc_comp_pids = dict()
gpp_comp_pids = dict()
for e in nfsdb:
    if any((u for u in config["gcc_spec"] if fnmatch.fnmatch(maybe_compiler_binary(e["b"]),u))):
        gcc_comp_pids[e["p"]] = e
    if any((u for u in config["gcc_spec"] if fnmatch.fnmatch(maybe_compiler_binary(e["b"]),u))):
        gpp_comp_pids[e["p"]] = e
gcc_execs = gcc_comp_pids.values()
gpp_execs = gpp_comp_pids.values()

def clang_ir_generation(cmds):
    if "-x" in cmds:
        if cmds[cmds.index("-x")+1]=="ir":
            return True
    return False

def clang_pp_input(cmds):
    if "-x" in cmds:
        if cmds[cmds.index("-x")+1]=="cpp-output":
            return True
    return False

def have_integrated_cc1(exe):
    if os.path.realpath(os.path.join(exe[1],exe[0])) in integrated_clang_compilers:
        if "-fno-integrated-cc1" not in exe[2]:
            return True
    return False

def is_ELF_executable(path):
    if os.path.exists(path):
        path = os.path.realpath(path)
        pn = subprocess.Popen(["file",path],shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        out,err = pn.communicate("")
        outFile = set([x.strip(",") for x in out.decode("utf-8").split()])
        return "ELF" in outFile and "executable" in outFile
    else:
        return False

def is_ELF_interpreter(path):
    if os.path.exists(path):
        path = os.path.realpath(path)
        pn = subprocess.Popen(["file",path],shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        out,err = pn.communicate("")
        outFile = set([x.strip(",") for x in out.decode("utf-8").split()])
        return "ELF" in outFile and "interpreter" in outFile
    else:
        return False

comp_pids = set()
clang_execs = [copy.deepcopy(e) for e in nfsdb if any((u for u in config["clang_spec"] if fnmatch.fnmatch(maybe_compiler_binary(e["b"]),u))) and ("-cc1" in e["v"] or (have_integrated_cc1((e["b"],e["w"],e["v"])) and "-c" in e["v"])) and "-o" in e["v"]\
  and ("-emit-llvm-bc" not in e["v"] or allow_llvm_bc) and not clang_ir_generation(e["v"]) and not clang_pp_input(e["v"]) and "-analyze" not in e["v"] and e["p"] not in comp_pids and comp_pids.add(e["p"]) is None]
clangpp_execs = [copy.deepcopy(e) for e in nfsdb if any((u for u in config["clangpp_spec"] if fnmatch.fnmatch(maybe_compiler_binary(e["b"]),u))) and ("-cc1" in e["v"] or (have_integrated_cc1((e["b"],e["w"],e["v"])) and "-c" in e["v"])) and "-o" in e["v"]\
  and ("-emit-llvm-bc" not in e["v"] or allow_llvm_bc) and not clang_ir_generation(e["v"]) and not clang_pp_input(e["v"]) and "-analyze" not in e["v"] and e["p"] not in comp_pids and comp_pids.add(e["p"]) is None]
clang_pattern_match_execs = [e for e in nfsdb  if any((u for u in config["clang_spec"] if fnmatch.fnmatch(maybe_compiler_binary(e["b"]),u)))]
clangpp_pattern_match_execs = [e for e in nfsdb  if any((u for u in config["clangpp_spec"] if fnmatch.fnmatch(maybe_compiler_binary(e["b"]),u)))]

gcc_input_execs = [e for e in gcc_execs if not "-" in e["v"]]
gpp_input_execs = [e for e in gpp_execs if not "-" in e["v"]]
clang_input_execs = [e for e in clang_execs if not e["v"][-1]=="-"]
clangpp_input_execs = [e for e in clangpp_execs if not e["v"][-1]=="-"]
clang_pattern_match_input_execs = [e for e in clang_pattern_match_execs if not e["v"][-1]=="-"]
clangpp_pattern_match_input_execs = [e for e in clangpp_pattern_match_execs if not e["v"][-1]=="-"]

gxx_input_execs = gcc_input_execs + gpp_input_execs
# In clang installation on Ubuntu both clang and clang++ points to the same /usr/lib/llvm-X/bin/clang
# which makes confusion later when C++ command line is parsed by detected C compiler
# Write here explicitly if this is C compilation (1) or C++ (2)
clangxx_input_execs = [(x,1) for x in clang_input_execs] + [(x,2) for x in clangpp_input_execs]

gcc_compilers = [u for u in list(set([os.path.join(x["w"],maybe_compiler_binary(x["b"])) for x in gcc_input_execs])) if is_ELF_executable(u)]
gpp_compilers = [u for u in list(set([os.path.join(x["w"],maybe_compiler_binary(x["b"])) for x in gpp_input_execs])) if is_ELF_executable(u)]
gxx_compilers = set(gcc_compilers+gpp_compilers)
clang_compilers = [u for u in list(set([os.path.join(x["w"],maybe_compiler_binary(x["b"])) for x in clang_input_execs])) if is_ELF_executable(u) or is_ELF_interpreter(u)]
clangpp_compilers = [u for u in list(set([os.path.join(x["w"],maybe_compiler_binary(x["b"])) for x in clangpp_input_execs])) if is_ELF_executable(u) or is_ELF_interpreter(u)]
clang_pattern_match_compilers = [u for u in list(set([os.path.join(x["w"],maybe_compiler_binary(x["b"])) for x in clang_pattern_match_input_execs])) if is_ELF_executable(u) or is_ELF_interpreter(u)]
clangpp_pattern_match_compilers = [u for u in list(set([os.path.join(x["w"],maybe_compiler_binary(x["b"])) for x in clangpp_pattern_match_input_execs])) if is_ELF_executable(u) or is_ELF_interpreter(u)]
clangxx_compilers = set(clang_compilers+clangpp_compilers)

gxx_input_execs = [x for x in gxx_input_execs if os.path.join(x["w"],maybe_compiler_binary(x["b"])) in gxx_compilers]
clangxx_input_execs = [(xT[0],xT[1]) for xT in clangxx_input_execs if os.path.join(xT[0]["w"],maybe_compiler_binary(xT[0]["b"])) in clangxx_compilers]

# For clang compiler invocation that uses integrated cc1 calls (clang 10 and above without -fno-integrated-cc1 option (we also assume that CCC_OVERRIDE_OPTION is not used))
#  replace the command with underlying (in-process) cc1 command call to be further processed by compilation processor
def safe_remove(filename):
        try:
            os.remove(filename)
        except OSError:
            pass
    
safe_remove('cc1_replace.pkl')
safe_remove('cc1_replace_result2.pkl')

better_clangxx_input_execs = [
    dict(
            { 
                key: entry[0][key] 
                for key in ["b", "w", "v", "p", "x"]
            }, **{"t":entry[1]}
        ) for entry in clangxx_input_execs
]

print('pickle dump start')
with open('cc1_replace.pkl', 'wb') as f:
    cPickle.dump({
        'clangxx_input_execs': better_clangxx_input_execs,
        'integrated_clang_compilers': integrated_clang_compilers,
    }, f)
print('pickle dump end')

scriptDir = os.path.dirname(os.path.realpath(__file__))
proc = subprocess.Popen(['python3', scriptDir + '/get_clang_comps.py', 'cc1_replace'], shell=False)
proc.communicate()

print('pickle load start')
with open('cc1_replace_result2.pkl','rb') as f:
    better_clangxx_input_execs = cPickle.load(f)
print('pickle load end')

clangxx_input_execs = [xT[0] for xT in clangxx_input_execs]
for i, entry in enumerate(better_clangxx_input_execs):
    for key in ["b", "w", "v", "p", "x", "t","i"]:
        clangxx_input_execs[i][key] = entry[key]

safe_remove('cc1_replace.pkl')
safe_remove('cc1_replace_result2.pkl')

print ("\n--- replaced cc1 calls [%.2fs]"%(time.time()-start_time))
start_time = time.time()


if True:
    print ("gcc matches: %d"%(len(gcc_execs)))
    print ("g++ matches: %d"%(len(gpp_execs)))
    print ("gcc/++ inputs: %d"%(len(gxx_input_execs)))
    print ("gcc compilers: ")
    for x in gcc_compilers:
        print ("  %s"%(x))
    print ("g++ compilers: ")
    for x in gpp_compilers:
        print ("  %s"%(x))
    print ("clang pattern matching compilers: ")
    for x in clang_pattern_match_compilers:
        print ("  %s"%(x))
    print ("clang++ pattern matching compilers: ")
    for x in clangpp_pattern_match_compilers:
        print ("  %s"%(x))
    print ("clang matches: %d"%(len(clang_execs)))
    print ("clang++ matches: %d"%(len(clangpp_execs)))
    print ("clang/++ inputs: %d"%(len(clangxx_input_execs)))
    print ("clang compilers: ")
    for x in clang_compilers:
        print ("  %s"%(x))
    print ("clang++ compilers: ")
    for x in clangpp_compilers:
        print ("  %s"%(x))
    print ("integrated clang compilers: ")
    for x in integrated_clang_compilers:
        print ("  %s"%(x))

def get_clang_compilations_parallel(clang_c, clangxx_input_execs, internal_jobs):
    scriptDir = os.path.dirname(os.path.realpath(__file__))

    def safe_remove(filename):
        try:
            os.remove(filename)
        except OSError:
            pass

    def clang_executor(p, clangxx_input_execs, start_offset, conn):
        inFile, outFile = ('/dev/shm/clangxx_' + str(p) + '.pkl', '/dev/shm/clangxx_result_' + str(p) + '.pkl', )

        with open(inFile, 'wb') as f:
            cPickle.dump({
                'clang_tailopts': config["clang_tailopts"],
                'clangxx_input_execs': clangxx_input_execs,
                'clang_compilers': clang_compilers,
                'clangpp_compilers': clangpp_compilers,
                'integrated_clang_compilers': integrated_clang_compilers,
                'start_offset': start_offset,
                'debug': clang_c.debug,
                'verbose': clang_c.verbose,
                'debug_compilations': clang_c.debug_compilations,
                'allow_pp_in_compilations': clang_c.allow_pp_in_compilations,
                'chunk_number': p
            }, f)

        proc = subprocess.Popen(
            ['taskset', '-c', str(p), 'python3', scriptDir + '/get_clang_comps.py', 'clang', inFile, outFile], 
            shell=False)
        proc.communicate()

        with open(outFile, 'rb') as f:
            result = cPickle.load(f)

        safe_remove(inFile)
        safe_remove(outFile)
        conn.send((p, result))

    print("Searching for clang compilations ... (%d candidates; %d jobs)" % (len(clangxx_input_execs),internal_jobs))
    m = float(len(clangxx_input_execs))/internal_jobs
    job_list = []
    pipe_list = []
    for i in range(internal_jobs):
        x = clangxx_input_execs[int(m*i):int(m*(i+1))]
        recv_conn, send_conn = multiprocessing.Pipe(False)
        p = multiprocessing.Process(target=clang_executor, args=(i, x, int(m*i), send_conn))
        job_list.append(p)
        pipe_list.append(recv_conn)
        p.start()
    for x in pipe_list:
        r = x.recv()
        job_list[ r[0] ].join()
        job_list[ r[0] ] = r[1]
    print()
    return job_list


def get_gcc_compilations_parallel(gcc_c,gxx_input_execs,internal_jobs,input_compiler_parser):

    def gcc_executor(p,gcc_c,gxx_input_execs,start_offset,jobs,input_compiler_parser,conn):
        plugin_path = os.path.join(os.path.dirname(sys.argv[0]),"../libgcc_input_name.so")
        plugin_path = "-fplugin=" + os.path.realpath(plugin_path)
        if input_compiler_parser is None:
            comp_dict_gcc = gcc_c.get_compilations(gxx_input_execs,jobs,plugin_path)
        else:
            comp_dict_gcc = gcc_c.get_compilations(gxx_input_execs,jobs,plugin_path,input_compiler_parser)
        conn.send((p,{ x+start_offset:y for x,y in comp_dict_gcc.items() }))

    jobs = 2*internal_jobs
    jobs_per_run = float(len(gxx_input_execs))/jobs
    if jobs_per_run < float(16):
        jobs = int(math.floor(float(len(gxx_input_execs))/16)) if len(gxx_input_execs)>=16 else 1
    print ("Searching for gcc compilations ... (%d candidates; %d jobs)"%(len(gxx_input_execs),jobs))
    m = float(len(gxx_input_execs))/jobs
    job_list = []
    pipe_list = []
    for i in range(jobs):
        x = gxx_input_execs[int(m*i):int(m*(i+1))]
        recv_conn, send_conn = multiprocessing.Pipe(False)
        p = multiprocessing.Process(target=gcc_executor, args=(i,gcc_c,x,int(m*i),internal_jobs,input_compiler_parser,send_conn))
        job_list.append(p)
        pipe_list.append(recv_conn)
        p.start()
    for x in pipe_list:
        r = x.recv()
        job_list[ r[0] ].join()
        job_list[ r[0] ] = r[1]
    return job_list


def create_compilation_input_map(comp_dict,compiler_input_execs,gcc_compiler,clang_compiler):

    vdc = dict()
    os.system('setterm -cursor off')
    sys.stdout.write("Processing compilations... 0/%d"%(len(comp_dict)))
    i=0
    for k,v in comp_dict.items():
        sys.stdout.write("\rProcessing compilations... %d/%d"%(i,len(comp_dict)))
        i+=1
        
        cmd = compiler_input_execs[k][2]
        compiler_path = os.path.join(compiler_input_execs[k][1],maybe_compiler_binary(compiler_input_execs[k][0]))
        is_not_integrated_compiler = compiler_input_execs[k][5]

        if any((u for u in config["gcc_spec"] if fnmatch.fnmatch(compiler_path,u))) or\
         any((u for u in config["gpp_spec"] if fnmatch.fnmatch(compiler_path,u))):
            compiler = gcc_compiler

        if any((u for u in config["clang_spec"] if fnmatch.fnmatch(compiler_path,u))) or\
         any((u for u in config["clangpp_spec"] if fnmatch.fnmatch(compiler_path,u))):
            compiler = clang_compiler

        comp_fns = compiler.get_compiled_files(v,compiler_input_execs[k])

        pid = compiler_input_execs[k][3]
        exeidx = compiler_input_execs[k][4]
        comp_exe = pm[pid][exeidx]
        comp_objs = compiler.get_object_files(comp_exe,fork_map,rev_fork_map,wr_map)

        if len(comp_fns)>0:
            
            try:
                includes,defs,undefs = compiler.parse_defs(v,compiler_input_execs[k])
                includes = [os.path.realpath(os.path.normpath(os.path.join(compiler_input_execs[k][1],x))) for x in includes]
                ipaths = compiler.compiler_include_paths(compiler_path)
                if ipaths:
                    for u in ipaths:
                        if u not in includes:
                            includes.append(u)
                ifiles = compiler.parse_include_files(compiler_input_execs[k],includes)
            except Exception as e:
                with open(".nfsdb.log.err", "a") as ef:
                    ef.write("Exception while processing compilation:\n%s\n"%(" ".join(compiler_input_execs[k][2])))
                    ef.write("%s\n\n"%(str(e)))
                continue

            compiler_type = compiler.compiler_type(compiler_path)
            if compiler_type is not None:
                for fn in comp_fns:
                    src_type = compiler.get_source_type(cmd,compiler_type,os.path.splitext(fn)[1])
                    absfn = os.path.realpath( os.path.normpath( os.path.join(compiler_input_execs[k][1],fn) ) )
                    if absfn in vdc:
                        vdc[absfn].append( (includes,defs,undefs,ifiles,src_type,compiler_input_execs[k][3],compiler_input_execs[k][4],comp_objs,is_not_integrated_compiler) )
                    else:
                        vdc[absfn] = [ (includes,defs,undefs,ifiles,src_type,compiler_input_execs[k][3],compiler_input_execs[k][4],comp_objs,is_not_integrated_compiler) ]
    sys.stdout.write("\n")
    os.system('setterm -cursor on')
    return vdc

def create_compilation_exe_map(comp_dict,compiler_input_execs,gcc_compiler,clang_compiler):

    cxm = dict()
    for k,v in comp_dict.items():

        cmd = compiler_input_execs[k][2]
        compiler_path = os.path.join(compiler_input_execs[k][1],maybe_compiler_binary(compiler_input_execs[k][0]))

        if any((u for u in config["gcc_spec"] if fnmatch.fnmatch(compiler_path,u))) or\
         any((u for u in config["gpp_spec"] if fnmatch.fnmatch(compiler_path,u))):
            compiler = gcc_compiler

        if any((u for u in config["clang_spec"] if fnmatch.fnmatch(compiler_path,u))) or\
         any((u for u in config["clangpp_spec"] if fnmatch.fnmatch(compiler_path,u))):
            compiler = clang_compiler

        cxm[k] = (compiler.get_compiled_files(v,compiler_input_execs[k]),v[1])

    return cxm

def merge_dicts(x, y):
    z = x.copy()
    z.update(y)
    return z

process_clang = True
clang_nexecs = []
comp_dict_clang = dict()
real_clang_comps = list()
clang_c = None
if process_clang and len(clangxx_input_execs)>0:
    clang_nexecs = [(e["b"],e["w"],e["v"],e["p"],e["x"],e["t"],int(e["i"]==0)) for e in clangxx_input_execs]
    clang_c = clang.clang(args.verbose,args.debug,clang_compilers,clangpp_compilers,integrated_clang_compilers,args.debug_compilations,args.allow_pp_in_compilations)

    os.system('setterm -cursor off')
    clangd = get_clang_compilations_parallel(clang_c,clang_nexecs,args.jobs)
    comp_dict_clang = functools.reduce(lambda x,y: merge_dicts(x,y),clangd)
    os.system('setterm -cursor on')

    print ("Found %d clang compilations (%d candidates)"%(len(comp_dict_clang),len(clang_nexecs)))
    comp_dict_clang = {k:([os.path.normpath(x) for x in v[0]],v[1]) for k,v in comp_dict_clang.items()}
    real_clang_comps = [ tuple(list(clang_nexecs[k])+[k]) for k in comp_dict_clang.keys() ]

key_offset = len(clangxx_input_execs)

print ("--- finished searching for clang compilations [%.2fs]"%(time.time()-start_time))
start_time = time.time()

process_gcc = True
gcc_nexecs = []
comp_dict_gcc = dict()
real_gcc_comps = list()
gcc_c = None
if process_gcc and len(gxx_input_execs)>0:
    gcc_nexecs = [(e["b"],e["w"],e["v"],e["p"],e["x"],1) for e in gxx_input_execs]
    gcc_c = gcc.gcc(args.verbose,args.debug,gcc_compilers,gpp_compilers,args.debug_compilations)
    os.system('setterm -cursor off')
    gccd = get_gcc_compilations_parallel(gcc_c,gcc_nexecs,args.jobs,None) ## TODO: add input compiler parser
    comp_dict_gcc = functools.reduce(lambda x,y: merge_dicts(x,y),gccd)
    os.system('setterm -cursor on')
    print ("Found %d gcc compilations (%d candidates)"%(len(comp_dict_gcc),len(gcc_nexecs)))
    comp_dict_gcc = {k+key_offset:([os.path.normpath(x) for x in v[0]],v[1]) for k,v in comp_dict_gcc.items()}
    real_gcc_comps = [ tuple(list(gcc_nexecs[k-key_offset])+[k]) for k in comp_dict_gcc.keys() ]

key_offset = len(clangxx_input_execs) + len(gxx_input_execs)

print ("--- finished searching for gcc compilations [%.2fs]"%(time.time()-start_time))

start_time = time.time()
compiler_nexecs = clang_nexecs + gcc_nexecs
real_compiler_comps = real_clang_comps + real_gcc_comps
comp_dict = dict()
comp_dict.update(comp_dict_clang)
comp_dict.update(comp_dict_gcc)

vdc = create_compilation_input_map(comp_dict,compiler_nexecs,gcc_c,clang_c)
print ("--- created compilations input map [%.2fs]"%(time.time()-start_time))
start_time = time.time()

parsed_comp_dict = create_compilation_exe_map(comp_dict,compiler_nexecs,gcc_c,clang_c)

for k,v in vdc.items():
    # k - compiled source file
    # v - list of compilation executions that compiled this specific file
    #     one source file can be compiled many times
    for ci in v:
        e = pm[ci[5]][ci[6]]
        cf = list()
        if "d" in e:
            cf+=e["d"]["f"]
        e["d"] = {
            "f":list(set([k]+cf)),
            "i":ci[0],
            "d":[{"n":di[0],"v":di[1]} for di in ci[1]],
            "u":[{"n":ui[0],"v":ui[1]} for ui in ci[2]],
            "h":ci[3],
            "s":ci[4],
            "o":ci[7],
            "p":ci[8]
        }
print ("--- computed compilations  [%.2fs]"%(time.time()-compilation_start_time))

start_time = time.time()
with open(args.db_path,"w") as f:
    f.write("[")
    for i,e in enumerate(nfsdb):
        if i>0:
            f.write(",\n")
        else:
            f.write("\n")
        f.write(json.dumps(e))
    f.write("\n]")
print ("--- updated parsed entries  [%.2fs]"%(time.time()-start_time))
