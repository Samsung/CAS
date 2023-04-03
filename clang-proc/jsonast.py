import os
import sys
import subprocess
import multiprocessing
import json
import argparse
import traceback
import re
import collections
import time
import shutil
import itertools
import new_merge
from datetime import datetime
from queue import Empty, Full
__version_string__ = "0.90"

# external use
def mkunique_compile_commands(dbpath,debug=False,dry_run=False,ignoreDirPathEntries=set()):

    with open(dbpath, "r") as f:
        db = json.loads(f.read())

    fns = dict()
    duplicates = dict()
    for i, u in enumerate(db):
        dirPathEntries = set(u["directory"].split(os.sep))
        if len(dirPathEntries & ignoreDirPathEntries) > 0:
            continue
        if u["file"] in fns:
            duplicates[i] = fns[u["file"]]
        else:
            fns[u["file"]] = i

    ndb = list()
    for i, u in enumerate(db):
        dirPathEntries = set(u["directory"].split(os.sep))
        if len(dirPathEntries & ignoreDirPathEntries) > 0:
            continue
        if i in duplicates:
            if debug:
                print("# Duplicated compilation of {}:\n{}\nOriginal: \n{}\n".format(u["file"],json.dumps(u,indent=4, separators=(',', ': ')),json.dumps(db[duplicates[i]],indent=4, separators=(',', ': ') )))
            print(f"Found duplicated compilation of {u['file']}")

        else:
            ndb.append(u)

    if dry_run:
        return 0

    os.rename(dbpath, dbpath+".bak")

    with open(dbpath,"w") as f:
        f.write(json.dumps(ndb,indent=4, separators=(',', ': ')))

    return 0

def make_unique_compile_commands(cdbfile):

    with open(cdbfile,"r") as f:
        db = json.loads(f.read())

    fns = dict()
    duplicates = list()
    for i,u in enumerate(db):
        if u["file"] in fns:
            duplicates.append((i,fns[u["file"]]))
        else:
            fns[u["file"]] = i

    debug = None
    for i,u in enumerate(duplicates):
        if debug:
            print("# Duplicated compilation of {}:\n{}\nOriginal: \n{}\n".format(db[u[0]]["file"], json.dumps(db[u[0]], indent=4, separators=(',', ': ')),
                                                                                 json.dumps(db[u[1]], indent=4, separators=(',', ': '))))
        print("Found duplicated compilation of {}".format(db[u[0]]["file"]))
        del db[u[0]-i]

    os.rename(cdbfile,cdbfile+".bak")

    with open(cdbfile,"w") as f:
        f.write(json.dumps(db,indent=4, separators=(',', ': ')))

def writeDebugJSON(jdb,logf="/tmp/log"):
    with open(logf,"w") as f:
        f.write(json.dumps(jdb, indent=4, sort_keys=False))

def debug_print(s,fd):
    rv = fd.write(f"{s}\n")
    fd.flush()
    return rv

def get_fdb_executor(p,fnst,start_offset,compdbf,procbin,fops_string,debug,conn,quiet):
    s = set()
    DB = {}
    rv=0
    for i,fnt in enumerate(fnst):
        fn = fnt[0]
        cwd = fnt[1]
        cmd = [procbin,"-p",compdbf,"-f","%s"%(fn),"-R",fops_string]
        if debug:
            print("RUNNING: {}".format(" ".join(cmd).replace("(","\\(").replace(")","\\)")))
        proc = subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,cwd=cwd)
        out, err = proc.communicate()
        rv+=1 if proc.returncode!=0 else 0
        try:
            DB[fn] = json.loads(out)
        except Exception as e:
            print(f"Failed to process ({fn})")
            print(e)
            print("----------------------------------------")
            print(out)
            print("----------------------------------------")
            traceback.print_exc()
            rv+=1
        if not quiet:
            sys.stdout.write("\r{}%".format(int(float(i+1)*100/len(fnst))))
            sys.stdout.flush()
    conn.send((p,(DB,rv)))

def get_cdb_executor_auto_merge(n,fnst,command,conn,quiet,debug,test,verbose,udir,exit_on_error,output_err,file_logs):

    tid = ""
    if debug:
        tid = f"[Thread {n}] "
    if file_logs:
        debug_fd = open(file_logs,"a")
    else:
        debug_fd = sys.stdout
    mJDB = None
    rv=0
    for i,fnt in enumerate(fnst):
        try:
            fn = fnt[0]
            cwd = fnt[1]
            args = command+["%s"%(fn)]
            if debug:
                debug_print("{}RUNNING: {}".format(tid," ".join(args).replace("(","\\(").replace(")","\\)")),debug_fd)
            proc = subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE,cwd=cwd)
            out, err = proc.communicate()
            rv+=1 if proc.returncode!=0 else 0
        except Exception as e:
            print("{}ERROR - Failed to run ({}) - [{}] - [msg: {}]".format(tid," ".join(args), n, e))
            with open(output_err,"a") as ferr:
                ferr.write("{}ERROR - Failed to run ({}) - [{}] - [msg: {}]\n".format(tid," ".join(args), n, e))
                ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
                ferr.write(traceback.format_exc()+"\n")
                ferr.write("--------------------\n\n")
            rv+=1
        if exit_on_error and rv>0: break
        should_continue = False
        try:
            jdb = json.loads(out)
            if udir:
                ufn = os.path.join(udir,os.path.basename(fn)+".json")
                if os.path.exists(ufn):
                    ufns = sorted([x for x in os.listdir(udir) if re.match(os.path.splitext(os.path.basename(fn))[0]+"(_\d+)?\..*\.json",x)]) # check re.match
                    basefn = os.path.splitext(ufns[-1][:-5])
                    if len(ufns)>1:
                        ufn = os.path.join(udir,"_".join(basefn[0].split("_")[:-1])+"_"+str(int(basefn[0].split("_")[-1])+1)+basefn[1]+".json")
                    else:
                        ufn = os.path.join(udir,basefn[0]+"_1"+basefn[1]+".json")
                writeDebugJSON(jdb,ufn)
        except Exception as e:
            print("{}ERROR - Failed to process ({}) - [{}] - [msg: {}]".format(tid,fn, n, e))
            print("  {}@RUNNING: {}\n".format(tid," ".join(args).replace("(","\\(").replace(")","\\)")))
            with open(output_err,"a") as ferr:
                ferr.write("{}ERROR - Failed to process ({}) - [{}] - [msg: {}]\n".format(tid,fn, n, e))
                ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
                ferr.write("OUT: {}\n".format(out))
                ferr.write("ERR: {}\n".format(err))
                ferr.write("{}RUNNING: {}\n".format(tid," ".join(args).replace("(","\\(").replace(")","\\)")))
                ferr.write(traceback.format_exc()+"\n")
                ferr.write("--------------------\n\n")
            if verbose:
                traceback.print_exc()
            if exit_on_error:
                sys.exit(1)
            rv+=1
            should_continue = True
        if exit_on_error and rv>0: break
        if should_continue:
            continue
        try:
            if udir:
                if os.path.exists(os.path.join(udir,"__thread_{}_iter_{}_jdba.json".format(n,i-1))):
                    os.remove(os.path.join(udir,"__thread_{}_iter_{}_jdba.json".format(n,i-1)))
                ufn = os.path.join(udir,"__thread_{}_iter_{}_jdba.json".format(n,i))
                writeDebugJSON(mJDB,ufn)
            mJDB = merge_json_ast(mJDB,jdb,quiet,debug,verbose,file_logs,test,None,n,exit_on_error)
        except Exception as e:
            print("{}ERROR - Failed to merge ({}) [{}] - [msg: {}]".format(tid, list(jdb["sources"][0].keys())[0], n, e))
            with open(output_err,"a") as ferr:
                ferr.write("{}ERROR - Failed to merge ({}) [{}] - [msg: {}]\n".format(tid, list(jdb["sources"][0].keys())[0], n, e))
                ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
                ferr.write(traceback.format_exc()+"\n")
                ferr.write("--------------------\n\n")
            if verbose:
                traceback.print_exc()
            if exit_on_error:
                sys.exit(1)
            rv+=1
        if exit_on_error and rv>0: break
        if not quiet:
            progress = int(float(i+1)*100/len(fnst))
            if progress % 5 == 0:
                print("#### Thread #{}: {}%".format(n, progress))
    if file_logs:
        debug_fd.close()

    conn.send((n,(mJDB,rv)))

def get_cdb_executor_fast_merge(n,task_q,command,conn,quiet,debug,test,verbose,udir,exit_on_error,output_err,file_logs):

    thread_time = datetime.now()
    #log_q.put("%s:Started thread\n"%(thread_time.strftime("%H:%M:%S.%f")))
    tid = ""
    if debug:
        tid = f"[Thread {n}] "
    if file_logs:
        debug_fd = open(file_logs,"a")
    else:
        debug_fd = sys.stdout
    mJDB = None
    rv=0
    count = 0
    send_conn = conn[0]
    recv_list = conn[1]
    sync_q    = conn[2] 
    jobs = len(recv_list)

       
    while True:
        task_time = datetime.now()
        try:
            fnt = task_q.get(timeout=0.001)
            #log_q.put("%s:Task: %s\n"%(task_time.strftime("%H:%M:%S.%f"),fnt[0]))
            count+=1
            fn = fnt[0]
            cwd = fnt[1]
            total = fnt[2]
            args=command+[f"{fn}"]
            if debug:
                debug_print("{}RUNNING: {}".format(tid," ".join(args).replace("(","\\(").replace(")","\\)")),debug_fd)
            proc = subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE,cwd=cwd)
            out, err = proc.communicate()
            #log_q.put("%s:Obtained database\n"%(datetime.now()-task_time))
            rv+=1 if proc.returncode!=0 else 0
        except Empty:
            if task_q.qsize() == 0:
                break
            else:
                continue
        except Exception as e:
            print ("{}ERROR - Failed to run ({}) - [{}] - [msg: {}]".format(tid," ".join(args), n, e))
            with open(output_err,"a") as ferr:
                ferr.write("{}ERROR - Failed to run ({}) - [{}] - [msg: {}]\n".format(tid," ".join(args), n, e))
                ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
                ferr.write(traceback.format_exc()+"\n")
                ferr.write("--------------------\n\n")
            rv+=1
        if exit_on_error and rv>0: break
        should_continue = False
        try:
            jdb = json.loads(out)
            #log_q.put("%s:Parsed database\n"%(datetime.now()-task_time))
            if udir:
                ufn = os.path.join(udir,os.path.basename(fn)+".json")
                if os.path.exists(ufn):
                    ufns = sorted([x for x in os.listdir(udir) if re.match(os.path.splitext(os.path.basename(fn))[0]+"(_\d+)?\..*\.json",x)])
                    basefn = os.path.splitext(ufns[-1][:-5])
                    if len(ufns)>1:
                        ufn = os.path.join(udir,"_".join(basefn[0].split("_")[:-1])+"_"+str(int(basefn[0].split("_")[-1])+1)+basefn[1]+".json")
                    else:
                        ufn = os.path.join(udir,basefn[0]+"_1"+basefn[1]+".json")
                writeDebugJSON(jdb,ufn)
        except Exception as e:
            print("{}ERROR - Failed to process ({}) - [{}] - [msg: {}]".format(tid,fn, n, e))
            print("  {}@RUNNING: {}\n".format(tid," ".join(args).replace("(","\\(").replace(")","\\)")))
            with open(output_err,"a") as ferr:
                ferr.write("{}ERROR - Failed to process ({}) - [{}] - [msg: {}]\n".format(tid,fn, n, e))
                ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
                ferr.write("OUT: {}\n".format(out))
                ferr.write("ERR: {}\n".format(err))
                ferr.write("{}RUNNING: {}\n".format(tid," ".join(args).replace("(","\\(").replace(")","\\)")))
                ferr.write(traceback.format_exc()+"\n")
                ferr.write("--------------------\n\n")
            if verbose:
                traceback.print_exc()
            if exit_on_error:
                sys.exit(1)
            rv+=1
            should_continue = True
        if exit_on_error and rv>0: break
        if should_continue:
            continue
        try:
            if udir:
                if os.path.exists(os.path.join(udir,"__thread_{}_iter_{}_jdba.json".format(n,i-1))):
                    os.remove(os.path.join(udir,"__thread_{}_iter_{}_jdba.json".format(n,i-1)))
                ufn = os.path.join(udir,"__thread_{}_iter_{}_jdba.json".format(n,i))
                writeDebugJSON(mJDB,ufn)
            mJDB = merge_json_ast(mJDB,jdb,quiet,debug,verbose,file_logs,test,None,n,exit_on_error)
            #log_q.put("%s:Merged database\n"%(datetime.now()-task_time))
        except Exception as e:
            print("{}ERROR - Failed to merge ({}) [{}] - [msg: {}]".format(tid, list(jdb["sources"][0].keys())[0], n, e))
            with open(output_err,"a") as ferr:
                ferr.write("{}ERROR - Failed to merge ({}) [{}] - [msg: {}]\n".format(tid, list(jdb["sources"][0].keys())[0], n, e))
                ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
                ferr.write(traceback.format_exc()+"\n")
                ferr.write("--------------------\n\n")
            if verbose:
                traceback.print_exc()
            if exit_on_error:
                sys.exit(1)
            rv+=1
        if exit_on_error and rv>0: break
        if not quiet:
            if (total+1) % 100 == 0:
                print("Processed {} files".format(total+1), file=sys.stderr)
        #log_q.put("%s:Finished task %s\n"%(datetime.now()-task_time,fnt[0]))
    if file_logs:
        debug_fd.close()
    #log_q.put("%s:Processed %d files\n"%(datetime.now()-thread_time,count))
    #print("Thread #%d: processed %d files")%(n,count)
    second_time = datetime.now()
    #log_q.put("%s:Second phase\n"%(second_time.strftime("%H:%M:%S.%f")))
    this_task = [n]
    retries =jobs/(jobs-n)*16
    retry = retries

    while True:
        try:
            read_task = sync_q.get_nowait()
            chunk = read_task[0]
            #log_q.put("%s:Receive chunk %d\n"%(task_time.strftime("%H:%M:%S.%f"),chunk))
            jdb = recv_list[chunk].recv()
            #log_q.put("%s:Received chunk %d\n"%(datetime.now()-task_time,chunk))
            mJDB = merge_json_ast(mJDB,jdb,quiet,debug,verbose,file_logs,test,None,n,exit_on_error)
            #log_q.put("%s:Merged chunk %d\n"%(datetime.now()-task_time,chunk))
            print("Thread #{} merged chunk #{}".format(n,chunk))
            this_task.extend(read_task)
            continue
        except Empty:
            if retry:
                retry-=1
                continue
            else:
                retry = retries
                pass
        try:
            if not sync_q.empty():
                continue
            sync_q.put_nowait(this_task)
            #log_q.put("%s:Send chunk %d\n"%(task_time.strftime("%H:%M:%S.%f"),n))
            print(f"Thread #{n} send chunk", file=sys.stderr)
            send_conn.send(mJDB)
            #log_q.put("%s:Sent chunk %d\n"%(datetime.now()-task_time,n))
            print(f"Thread #{n} sent chunk", file=sys.stderr)
            send_conn.close()
            #log_q.put("%s:Finished second phase\n"%(datetime.now()-second_time))
            #log_q.put("%s:Finished thread\n"%(datetime.now()-thread_time))
            return
        except Full:
            pass

def merge_fops_db(fdba,fdbb,quiet=False,debug=False,test=False):

    if fdba is None:
        if fdbb["varn"]<=0:
            return None
        membern = sum([len(x["members"]) for x in fdbb["vars"]])
        if not quiet:
            print("Base file: [{}], vars: {}, members: {}".format(list(fdbb["sources"][0].keys())[0],fdbb["varn"],membern))
        fdbb["membern"] = membern
        return fdbb

    if fdbb["varn"]<=0:
        # Nothing to merge
        return fdba

    membern = sum([len(x["members"]) for x in fdbb["vars"]])
    fdba["sourcen"] = fdba["sourcen"]+1
    fdba["membern"] = fdba["membern"]+membern
    new_file_id = list(fdba["sources"][-1].values())[0]+1
    fdba["sources"] = fdba["sources"]+[{list(fdbb["sources"][0].keys())[0]:new_file_id}]
    fdba["vars"]+=fdbb["vars"]
    fdba["varn"] = len(fdba["vars"])

    if not quiet:
        print("New variables: {}, new members: {} [{}], sources: {}, vars: {}, members: {}".format(fdbb["varn"],membern,list(fdbb["sources"][0].keys())[0],fdba["sourcen"],fdba["varn"],fdba["membern"]))

    return fdba

# depreceated
def merge_multiple_json_ast(databases, quiet=False,debug=False,verbose=False,file_logs=None,test=False,exit_on_error=False,max_jobs = 0):
    # Merge all chunks using multiple processes { at most len(databases)/2 }
    # Chunks are merged 2 at a time. For chunks 1-8, 4 processes will in parallel merge 1 with 2; 3 with 4 etc.
    # Than 2 processes will merge 12 with 34 and 56 with 78, and than finall process will merge 1234 with 5678
    retries = 0
    global data
    data = databases
    if max_jobs == 0:
        max_jobs = multiprocessing.cpu_count()
    while len(data) > 1:
        result_list = []
        pool = multiprocessing.Pool(processes=max_jobs, maxtasksperchild=1)
        for i in range(0,len(data),2):
            if i + 1 < len(data):
                res = pool.apply_async(merge_json_proxy, args=(i, i+1, quiet,debug,verbose,file_logs,test,None,None,exit_on_error,max_jobs,))
            else:
                res = pool.apply_async(merge_json_proxy, args=(None, i, quiet,debug,verbose,file_logs,test,None,None,exit_on_error,max_jobs,))
            result_list.append(res)
        next_step_databases = []
        try:
            for res in result_list:
                next_step_databases.append(res.get())
            del data[:]
            pool.terminate()
            del pool
            data = next_step_databases
        except Exception as ex:
            retries += 1
            print(ex)
            if retries < 5:
                print("Merging failed {} times. Retrying.".format(retries))
                continue
            else:
                print("Merging failed {} times. The program will be aborted.".format(retries))
                return None
    return data[0]

# deprecated
def merge_json_proxy(ida,idb,quiet=False,debug=False,verbose=False,file_logs=None,test=False,chunk=None,threadid=None,exit_on_error=False,job_count=multiprocessing.cpu_count()):
    jdbb = data[idb]
    jdba = None
    if ida != None:
        jdba = data[ida]
    ret = merge_json_ast(jdba, jdbb, copy_jdba = False)
    return ret

# Function proxy; set in create_json_db main
def merge_json_ast(jdba,jdbb,quiet=False,debug=False,verbose=False,file_logs=None,test=False,chunk=None,threadid=None,exit_on_error=False,job_count=multiprocessing.cpu_count(),copy_jdba=True):
    return 

def make_ordered_jdb(jdb):
    serialized_jdb = list()
    for k,v in jdb.items():
        if k=="funcs" or k=="types":
            serialized_jdb.append((k,sorted(v,key = lambda x: x["id"])))
        else:
            serialized_jdb.append((k,v))
    return collections.OrderedDict(serialized_jdb)


def create_json_db_main(args: argparse.Namespace, allowed_phases: dict) -> int:

    script_dir = os.path.dirname(os.path.abspath(__file__))

    if not os.path.isabs(args.proc_binary):
        print("Please provide full path to the clang processor binary (not {})".format(args.proc_binary))
        return 1

    if not args.field_usage:
        args.field_usage = True

    if not args.taint:
        args.taint = os.path.join(script_dir,"dbtaint.json")

# select merging version
    global merge_json_ast
    merge_json_ast = new_merge.merge_json_ast

# select phase [to be removed]
    if args.phase is None:
        args.phase = "db"
    
    phase = None
    if args.phase:
        if args.phase.lower() not in set(list(allowed_phases.keys())):
            print("Invalid execution phase: {}".format(args.phase))
            return 1
        else:
            phase = args.phase.lower()

    if phase=="db":
        if args.fops_database:
            fops_database = os.path.realpath(args.fops_database)        
        else:
            fops_database = None

# output
    if not args.output:
        if phase=="fops":
            output = "fops.json"
        else:
            output = "db.json"
    else:
        output = args.output

    output_err = output+".err"
    if args.clean_slate:
        if os.path.exists(output_err):
            os.remove(output_err)

    if args.forward_output:
        orig_stdout = sys.stdout
        orig_stderr = sys.stderr
        f = open(args.forward_output, 'w')
        sys.stdout = f
        sys.stderr = f

# target files
    if not args.compilation_database:
        compdbf = os.path.join(os.getcwd(),"compile_commands.json")
    else:
        compdbf = os.path.realpath(args.compilation_database)

    cdbd = {}
    with open(compdbf,"r") as f:
        compdb = json.loads(f.read())
        duplicates = list()
        for i,u in enumerate(compdb):
            fn = os.path.join(u["directory"],u["file"])
            if fn in cdbd:
                duplicates.append((i,fn))
            else:
                cdbd[fn] = u["directory"].encode("ascii")

    for u in duplicates:
        print("Ignoring duplicated compilation of {}".format(u[1]))
    
    if args.unique_cdb:
        make_unique_compile_commands(compdbf)

    if args.compilation_list:
        with open(args.compilation_list,"r") as f:
            fns = [x.strip() for x in f.readlines() if x!=""]
    else:
        fns = cdbd.keys()
        
    if args.range:
        fns = fns[int(args.range.split(":")[0]):int(args.range.split(":")[1])]

    if len(fns)<=0:
        with open(output,"w") as f:
            f.write(json.dumps({}))
        print("Created empty JSON database")
        return 0

    fns = [(x,cdbd[x],i) for i, x in enumerate(fns)]

# modules
    if args.compilation_dependency_map:
        with open(args.compilation_dependency_map,"rb") as f:
            cdm = json.loads(f.read())
        rcdm = {}
        for m,srcLst in cdm.items():
            for src in srcLst:
                if src in rcdm:
                    rcdm[src].append(m)
                else:
                    rcdm[src] = [m]
                    
    if args.compilation_dependency_map and len(cdm)>1:
        os.environ["__CREATE_JSON_DB_MULTIPLE_MODULE_OPTION__"] = "true"

    # It looks like there are cases when building vmlinux we have functions with the same signature and their symbols are changed by objcopy with --prefix-symbols
    #  and that's how it links but dbjson cannot know about that => allow for having multiple signatures for some function even for single module
    os.environ["__CREATE_JSON_DB_MULTIPLE_MODULE_OPTION__"] = "true"

# building command for clang-proc
    command = []
    command.append(args.proc_binary)
    if not args.skip_body:
        command.append("-b")
    if not args.skip_switches:
        command.append("-s")
    if args.taint:
        command.append("-pt")
        command.append(args.taint)
    if args.include_path:
        command.append("-i")
    if args.processor_error:
        command.append("-E")
    if not args.skip_defs:
        command.append("-F")
    if args.field_usage:
        command.append("-csd")
    if args.with_cta:
        command.append("-a")
    if args.enable_static_assert:
        command.apend("-sa")
    MR = {}
    if args.macro_replacement:
        with open(args.macro_replacement,"rb") as f:
            MR = json.loads(f.read())
            for k,v in MR.items():
                command.append("-M")
                command.append(f"{k}:{v}")
    ME = list()
    if args.macro_expansion:
        command.append("-X")

    addDefs = list()
    if args.additional_defines:
        with open(args.additional_defines,"rb") as f:
            addDefs = json.loads(f.read())
            for df in addDefs:
                command.append("-D")
                command.append(f"{df}")
    
    if args.additional_include_paths:
        for ipath in args.additional_include_paths:
            command.append("-A")
            command.append(f"{ipath}")

    command+=["-c","-t","-L","-p",compdbf]

# execution
    if args.jobs is not None and args.jobs>0:
        jobs = args.jobs
    else:
        jobs = multiprocessing.cpu_count()
    jobs = jobs if len(fns)>=jobs else len(fns)

    FDB = None

    if phase=="fops" or phase is None:

        print(f"Creating fops database from {len(fns)} sources...")

        if args.fops_record:
            fops_string = ":".join(args.fops_record)
        else:
            fops_string = "*"

        m = float(len(fns))/jobs
        job_list = []
        pipe_list = []
        for i in range(jobs):
            x = fns[int(m*i):int(m*(i+1))]
            recv_conn, send_conn = multiprocessing.Pipe(False)
            dr = os.path.dirname(compdbf)
            p = multiprocessing.Process(target=get_fdb_executor, args=(i,x,int(m*i),dr if dr!="" else ".",args.proc_binary,fops_string,args.debug,send_conn,args.quiet))
            job_list.append(p)
            pipe_list.append(recv_conn)
            p.start()

        rv = 0
        for x in pipe_list:
            r = x.recv()
            job_list[ r[0] ].join()
            fopsdbs,erv = r[1]
            rv+=erv

            if len(fopsdbs)>0:
                for fdb in fopsdbs.values():
                    try:
                        FDB = merge_fops_db(FDB,fdb,args.quiet,args.debug,args.test)
                    except Exception as e:
                        print("Failed to merge ({})".format(list(fdb["sources"][0].keys())[0]))
                        print(e)
                        traceback.print_exc()
                        rv+=1
            else:
                pass

        print()
        if not args.quiet and FDB is not None:
            print("sources: {}, vars: {}, members: {}".format(FDB["sourcen"],FDB["varn"],FDB["membern"]))

        if phase=="fops":
            fops_output = output
        elif args.save_intermediates:
            fops_output = "fops.json"
        
        if phase=="fops" or args.save_intermediates:
            with open(fops_output, "w") as f:
                f.write(json.dumps(FDB,indent=4))
            print("Done. Written {} [{:.2f}MB]".format(fops_output,float(os.stat(fops_output).st_size)/1048576))
            fops_database = fops_output

        if phase=="fops":
            return rv
    
    if phase!="db" and phase is not None:
        return 0


    module_info_string = ""
    if args.compilation_dependency_map and len(cdm)>1:
        module_info_string = f" ({len(cdm)} modules)"
        assert os.environ["__CREATE_JSON_DB_MULTIPLE_MODULE_OPTION__"] == "true"

    print("Creating JSON database from {} sources {}...".format(len(fns),module_info_string))

    if FDB is None:
        if fops_database:
            with open(fops_database,"r") as f:
                FDB = json.loads(f.read())
        else:
            FDB = {"sources": [],"membern":0,"varn":0,"sourcen":0,"vars":[]}

    udir = None
    if args.unmerged_temp and args.clean_slate:
        if os.path.exists(args.unmerged_temp):
            shutil.rmtree(args.unmerged_temp)
    if args.unmerged_temp:
        if not os.path.exists(args.unmerged_temp):
            try:
                os.makedirs(args.unmerged_temp)
                udir = args.unmerged_temp
            except Exception as e:
                print("Failed to create director for unmerged temporary files ({}) Ignoring...".format(args.unmerged_temp))
        else:
            udir = args.unmerged_temp

    if args.clean_slate:
        if args.file_logs and os.path.exists(args.file_logs):
            os.remove(args.file_logs)

    chunk_list = None
    mrrs = 0
    if args.only_merge:
        if os.path.exists(args.only_merge):
            chunk_list = sorted([os.path.join(args.only_merge,x) for x in os.listdir(args.only_merge) if re.match("chunk_\d+\.json",x)],key=lambda x: int(os.path.splitext(os.path.basename(x))[0].split("_")[-1])) # check re.match
            JDB = None
            for i,fn in enumerate(chunk_list):
                with open(fn,"r") as f:
                    jsondbr = json.loads(f.read())
                try:
                    JDB = merge_json_ast(JDB,jsondbr,args.quiet,args.debug,args.verbose,args.file_logs,args.test,i)
                    if args.save_chunk_merge_temporary:
                        with open(os.path.join(args.only_merge,"__chunk_merged.json"),"w") as f:
                            f.write(json.dumps(JDB))
                except Exception as e:
                    print("ERROR - Failed to merge chunk {} ({}) - [msg: {}]".format(i, fn, e))
                    with open(output_err,"a") as ferr:
                        ferr.write("ERROR - Failed to merge chunk {} ({}) - [msg: {}]\n".format(i, fn, e))
                        ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
                        ferr.write(traceback.format_exc()+"\n")
                        ferr.write("--------------------\n\n")
                    if args.verbose:
                        traceback.print_exc()
                    if args.exit_on_error:
                        sys.exit(1)
                    mrrs+=1
        else:
            print("Cannot find directory that contains chunks to merge: {}".format(args.only_merge))
            return 1
        if not args.quiet and JDB is not None:
            print("sources: {}, functions: {}, f. declarations: {}, unresolved functions: {}, types: {}, merging errors: {}".format(JDB["sourcen"], JDB["funcn"], JDB["funcdecln"], JDB["unresolvedfuncn"], JDB["typen"], mrrs))
        wfn = os.path.join(args.only_merge,"merged.json")
        writeDebugJSON(make_ordered_jdb(JDB),wfn)
        return mrrs


    JDB = None
    rv = 0
    mrrs = 0

    if (args.only_embed_fops):
         with open(output,"r") as f:
            JDB = json.loads(f.read())
            del JDB["fops"]

    elif args.fast_merge:
        task_q = multiprocessing.Queue()
        sync_q = multiprocessing.Queue()
        map(task_q.put,fns)

        job_list = []
        recv_list = []
        send_list = []
        #log_list = []
        for i in range(jobs):
            recv_conn, send_conn = multiprocessing.Pipe(False)
            # log_q = multiprocessing.Queue()
            recv_list.append(recv_conn)
            send_list.append(send_conn)
            #log_list.append(log_q)
        for i in range(jobs):
            dr = os.path.dirname(compdbf)
            conn = (send_list[i],recv_list,sync_q)
            #log_q = log_list[i]
            p = multiprocessing.Process(target=get_cdb_executor_fast_merge, args=(i,task_q,command,conn,args.quiet,
                args.debug,args.test,args.verbose,udir,args.exit_on_error,output_err,args.file_logs))

            job_list.append(p)
            p.start()
        #logging handler
        # logger = threading.Thread(target=logger_task,args=(log_list,))
        # logger.start()
        finished_all = sync_q.get()
        JDB = recv_list[finished_all[0]].recv()
        print("Merged chunk {}".format(finished_all[0]), file=sys.stderr)
        while True:
            finished_chunk = sync_q.get()
            jdb = recv_list[finished_chunk[0]].recv()
            JDB = merge_json_ast(JDB,jdb,args.quiet,args.debug,args.verbose,args.file_logs,args.test)
            print("Merged chunk {}".format(finished_chunk[0]), file=sys.stderr)
            finished_all.extend(finished_chunk)
            if len(finished_all) == jobs:
                break

        [p.join() for p in job_list]
        #log_list[0].put(None)
        #logger.join()
        #print("Printed log to thred_#id.log files")

    else:
        m = float(len(fns))/jobs
        job_list = []
        pipe_list = []
        # status: [0: ERROR (repeat) / 1: OK / 2: FAIL (give up), TRY_COUNT]
        status_list = [[0,0] for i in range(jobs)]
        json_list = []
        for i in range(jobs):
            if args.job_number is not None:
                if i!=args.job_number:
                    continue
            x = fns[int(m*i):int(m*(i+1))]
            recv_conn, send_conn = multiprocessing.Pipe(False)
            dr = os.path.dirname(compdbf)
            p = multiprocessing.Process(target=get_cdb_executor_auto_merge, args=(i if args.job_number is None else 0,x,command,
                send_conn,args.quiet,args.debug,args.test,args.verbose,udir,args.exit_on_error,output_err,args.file_logs))
            job_list.append(p)
            pipe_list.append(recv_conn)
            status_list[i][1] = 1
            json_list.append({})
            p.start()

        max_tries = 5
        while not all([x[0] for x in status_list]):
            for i,s in enumerate(status_list):
                if not s[0]:
                    x = pipe_list[i]
                    try:
                        r = x.recv()
                    except EOFError as e:
                        print("WARNING - EOF while reading from chunk {} - [msg: {}]".format(i, e))
                        p = job_list[i]
                        p.join()
                        with open(output_err,"a") as ferr:
                            ferr.write("WARNING - EOF while reading from chunk {} - [msg: {}]\n".format(i, e))
                            ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
                            ferr.write("returncode: {}".format(p.exitcode))
                            ferr.write(traceback.format_exc()+"\n")
                            ferr.write("--------------------\n\n")
                        # It failed for some unknown reason while reading from the pipe
                        # Reschedule and try to compute the chunk again (until max_tries reached)
                        if status_list[i][1]>=max_tries:
                            print("ERROR - giving up on reading from chunk {} after {} tries".format(i,max_tries))
                            rv+=1
                            status_list[i][0] = 2
                        else:
                            print(f"INFO - rescheduling computation on chunk {i}")
                            x = fns[int(m*i):int(m*(i+1))]
                            recv_conn, send_conn = multiprocessing.Pipe(False)
                            dr = os.path.dirname(compdbf)
                            p = multiprocessing.Process(target=get_cdb_executor_auto_merge, args=(i if args.job_number is None else 0,x,command,
                                send_conn,args.quiet,args.debug,args.test,args.verbose,udir,args.exit_on_error,output_err,args.file_logs))
                            job_list[i] = p
                            pipe_list[i] = recv_conn
                            status_list[i][1] += 1
                            json_list[i] = {}
                            p.start()
                        continue

                    job_list[ r[0] ].join()
                    jsondbr,erv = r[1]
                    json_list[ r[0] ] = jsondbr
                    status_list[ r[0] ][0] = 1
                    mrrs+=erv
        

        print(f"INFO - Errors reported in first phase of merging: {mrrs}")
        json_list_sanitized = []
        if args.parallel_merge != None:
            for i,s in enumerate(status_list):
                if status_list[i][0]<=1:
                    if json_list[i] is None:
                        continue
                json_list_sanitized.append(json_list[i])
            del json_list
            del status_list
            del jsondbr
            JDB = merge_multiple_json_ast(json_list_sanitized, args.quiet,args.debug,args.verbose,args.file_logs,args.test,args.exit_on_error,args.parallel_merge)
        else:
            for i,s in enumerate(status_list):
                if status_list[i][0]<=1:
                    jsondbr = json_list[i]

                    if jsondbr is None:
                        # Nothing to merge
                        continue

                    try:
                        if args.unmerged_temp:
                            ufn = os.path.join(args.unmerged_temp,f"chunk_{i}.json")
                            writeDebugJSON(make_ordered_jdb(jsondbr),ufn)
                        JDB = merge_json_ast(JDB,jsondbr,args.quiet,args.debug,args.verbose,args.file_logs,args.test,i,None,args.exit_on_error,jobs)
                        if args.unmerged_temp:
                            ufn = os.path.join(udir,f"__finalthread_iter_{i}_jdba.json")
                            writeDebugJSON(JDB,ufn)
                    except Exception as e:
                        print(f"ERROR - Failed to merge chunk {i} - [msg: {e}]")
                        with open(output_err,"a") as ferr:
                            ferr.write("ERROR - Failed to merge chunk {} - [msg: {}]\n".format(i, e))
                            ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
                            ferr.write(traceback.format_exc()+"\n")
                            ferr.write("--------------------\n\n")
                        if args.verbose:
                            traceback.print_exc()
                        if args.exit_on_error:
                            sys.exit(1)
                        rv+=1

    if FDB is None:
        FDB = {"membern":0,"vars":[]}
    if "vars" in FDB and len(FDB["vars"])>0:
        fdfrefmap = {}
        frefmap = {}
        fdrefmap = {}
        for x in JDB["funcs"]:
            fdfrefmap[x["declhash"]] = x
            frefmap[x["id"]] = x
        for x in JDB["funcdecls"]:
            if x["declhash"] not in fdfrefmap:
                fdfrefmap[x["declhash"]] = x
            fdrefmap[x["id"]] = x
        missing_ids = list()
        fids = list()
        fdids = list()
        for v in FDB["vars"]:
            nm = dict()
            for k,vv in v["members"].items():
                if vv not in fdfrefmap:
                    missing_ids.append(vv)
                else:
                    nm[k] = fdfrefmap[vv]["id"]
                    if nm[k] in frefmap:
                        fids.append(nm[k])
                    elif nm[k] in fdrefmap:
                        fdids.append(nm[k])
            v["members"] = nm
        print("Missing FOPS function ids: {}".format(len(missing_ids)))
        print("Number of FOPS functions: {}".format(len(fids)))
        print("Number of FOPS function declarations: {}".format(len(fdids)))

    JDB["fops"] = FDB

    if not args.quiet and JDB is not None:
        print("sources: {}, functions: {}, f. declarations: {}, unresolved functions: {}, types: {}, merging errors: {}".format(JDB["sourcen"], JDB["funcn"], JDB["funcdecln"], JDB["unresolvedfuncn"], JDB["typen"], mrrs+rv))

    if args.compilation_dependency_map:
        suppress_progress = False
        if "JENKINS_HOME" in os.environ and os.environ["JENKINS_HOME"]!="":
            suppress_progress = True
        print("Adding module/source mapping information for {} modules and {} functions...".format(len(cdm.keys()),len(JDB["funcs"])))
        JDB["modules"] = [{m:i} for i,m in enumerate(cdm.keys())]
        mMap = { list(mp.keys())[0]:list(mp.values())[0] for mp in JDB["modules"] }
        srcMap = { list(mp.values())[0]:list(mp.keys())[0] for mp in JDB["sources"] }
        if not suppress_progress:
            sys.stdout.write("0%")
            sys.stdout.flush()
        modified = 0
        failed = 0
        for i,f in enumerate(JDB["funcs"]):
            if not suppress_progress:
                sys.stdout.write("\r{}%".format(int((float(i+1)/len(JDB["funcs"]))*100)))
                sys.stdout.flush()
            srcLst = [srcMap[x] for x in f["fids"]]
            try:
                mLst = list(set(itertools.chain.from_iterable([rcdm[src] for src in srcLst])))
                f["mids"] = [mMap[m] for m in mLst]
                modified+=1
            except Exception as e:
                f["mids"] = []
                failed+=1
                with open(output_err,"a") as ferr:
                    ferr.write("Failed to add module info to function '{}'@{}\n".format(f["name"],f["id"]))
                    ferr.write("fids: {}\n".format(str(f["fids"])))
                    ferr.write("sources:\n")
                    for src in srcLst:
                        ferr.write("  {}\n".format(src))
                    ferr.write("corresponding modules:\n")
                    for src in srcLst:
                        if src in rcdm:
                            ferr.write("  {}\n".format(str(rcdm[src])))
                        else:
                            ferr.write("XXX\n")
                    ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
                    ferr.write(traceback.format_exc()+"\n")
                    ferr.write("--------------------\n\n")
        print()
        print("Modified {} functions (failed {})".format(modified,failed))
        modified = 0
        failed = 0
        for i,g in enumerate(JDB["globals"]):
            if not suppress_progress:
                sys.stdout.write("\r{}%".format(int((float(i+1)/len(JDB["globals"]))*100)))
                sys.stdout.flush()
            srcname = srcMap[g["fid"]]
            try:
                mLst = rcdm[srcname]
                g["mids"] = [mMap[m] for m in mLst]
                modified+=1
            except Exception as e:
                g["mids"] = []
                failed+=1
                with open(output_err,"a") as ferr:
                    ferr.write("Failed to add module info to global '{}'@{}\n".format(g["name"],g["id"]))
                    ferr.write("fid: {}\n".format(g["fid"]))
                    ferr.write("source: {}\n".format(srcname))
                    ferr.write("corresponding modules:\n")
                    if srcname in rcdm:
                        ferr.write("  {}\n".format(str(rcdm[srcname])))
                    else:
                        ferr.write("XXX\n")
                    ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
                    ferr.write(traceback.format_exc()+"\n")
                    ferr.write("--------------------\n\n")
        print()
        print("Modified {} globals (failed {})".format(modified,failed))

    JDB["version"] = __version_string__
    JDB["release"] = args.sw_version if args.sw_version else ""
    JDB["module"] = args.module_info if args.module_info else ""
    # Remap sources and modules arrays for better search in AoT
    JDB["source_info"] = [{"name":list(srcD.keys())[0],"id":list(srcD.values())[0]} for srcD in JDB["sources"]]
    if "modules" in JDB:
        JDB["module_info"] = [{"name":list(srcD.keys())[0],"id":list(srcD.values())[0]} for srcD in JDB["modules"]]
    # Now save the final JSON
    with open(output,"w") as f:
        f.write(json.dumps(JDB,indent=4))
    print("Done. Written {} [{:.2f}MB]".format(output,float(os.stat(output).st_size)/1048576))
    if mrrs+rv > 0:
        print("WARNING: Encountered some ERRORS!")
    
    if args.forward_output:
        sys.stdout = orig_stdout
        sys.stderr = orig_stderr
        f.close()
    
    return mrrs+rv
