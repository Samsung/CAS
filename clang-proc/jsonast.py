import os
import sys
import subprocess
import multiprocessing
import json
import argparse
import traceback
import time
import shutil
import itertools
from typing import Any, Optional, Iterable
from threading import Thread

__version_string__ = "0.90"

class progressbar:
    def __init__(self, *args, **kwargs):
        try:
            from tqdm import tqdm as __progressbar
        except:
            class __progressbar:
                n = 0
                x = None

                def __init__(self, data:Optional[Iterable]=None, x=None, total=0, disable=None,**kwargs):
                    self.data_iter = data.__iter__()
                    self.x = x
                    self.total = total
                    self.desc = kwargs["desc"] if 'desc' in kwargs else 'Progress'
                    if disable is None:
                        self.disable = not sys.stdout.isatty()
                    self.start_time = time.time()

                def __iter__(self):
                    return self
                def __next__(self):
                    self.refresh()
                    self.n+=1
                    return self.data_iter.__next__()

                def refresh(self):
                    if not self.disable:
                        self.render()

                def render(self):
                    progress = self.n/(self.total+0.0001)
                    rbar = f'{self.desc}: {100*progress:3.1f}%|'
                    lbar = f'| {self.n}/{self.total} [Elapsed time: {time.time()-self.start_time:.1f}s]'
                    mlen = shutil.get_terminal_size()[0] - len(rbar) - len(lbar)
                    fill = mlen*progress
                    fillchr = chr(0x258f - int(8*(fill - int(fill))))
                    mbar = f'{"":\u2588<{int(fill)}}{fillchr}{"":<{mlen-int(fill)-1}}'
                    print(f'\r\033[K{rbar}{mbar}{lbar}',end='')

                def close(self):
                    pass

        self.pb = __progressbar(*args, **kwargs)

    def __getattribute__(self, __name: str) -> Any:
        if __name == 'pb':
            return super().__getattribute__(__name)
        return getattr(self.pb, __name)
    def __setattr__(self, __name: str, __value: Any) -> None:
        if __name == 'pb':
            return super().__setattr__(__name,__value)
        return setattr(self.pb,__name,__value)

    def __iter__(self):
        return self.pb.__iter__()


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

def find_processor_binary(proc_bin):
    # user provided binary
    if proc_bin:
        if os.path.isdir(proc_bin):
            proc_bin = os.path.join(proc_bin,"clang-proc")
        proc_bin = os.path.abspath(proc_bin)
        if os.path.exists(proc_bin):
            return proc_bin
        else:
            print(f"Couldn't find expected clang processor binary: {proc_bin}")
            return None
    else:
        # try local directory
        proc_bin = os.path.abspath('./clang-proc')
        if os.path.exists(proc_bin):
            return proc_bin
        # try script directory
        proc_bin = os.path.abspath(os.path.join(os.path.dirname(__file__),"clang-proc"))
        if os.path.exists(proc_bin):
            return proc_bin
        # try path
        proc_bin = shutil.which("clang-proc")
        if proc_bin and os.path.exists(proc_bin):
            return proc_bin
        print(f"Couldn't find clang processor binary. Provide path with '-P' option.")
        return None

def create_json_db_main(args: argparse.Namespace,stream=False) -> int:
    
    args.proc_binary = find_processor_binary(args.proc_binary)
    if not args.proc_binary:
        return 1
    print(f"Using clang processor binary: {args.proc_binary}")


    if not args.field_usage:
        args.field_usage = True

    if not args.taint:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        args.taint = os.path.join(script_dir,"dbtaint.json")

# output
    if not args.output:
        output = "db.json"
        output_img = "db.img"
    else:
        output = args.output
        output_img = ''.join([os.path.splitext(args.output)[0],'.img'])

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
        fns = list(cdbd.keys())
        
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

    module_info_string = ""
    if args.compilation_dependency_map and len(cdm)>1:
        module_info_string = f" ({len(cdm)} modules)"
        assert os.environ["__CREATE_JSON_DB_MULTIPLE_MODULE_OPTION__"] == "true"

    print("Creating JSON database from {} sources {}...".format(len(fns),module_info_string))

    JDB = None
    rv = 0
    mrrs = 0


    command += ["-multi" ,"-tc", f"{jobs}"]
    if(args.range):
        for f in fns:
            command += ["%s"%(f[0])]
    else:
        command += ["__all__"]
    try:
        proc = subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        out = []
        err = ''
        def read_out(proc,out):
            out.append(proc.stdout.read())
            proc.stdout.close()

        t = Thread(target=read_out,args=[proc,out],)
        t.start()
        print(proc.stderr.readline()[5:],end='')
        count = 0
        log_iter = progressbar(proc.stderr,total=len(fns),desc="Parsing files",miniters=1,disable=None)
        log_iter.bar_format='{desc}: {percentage:3.1f}%|{bar}{r_bar}'
        for line in log_iter:
            if line.startswith("LOG: "):
                count+=1
                if count > log_iter.total:
                    log_iter.close()
                    print(line[5:],end='')
                    break
                continue
            else:
                err = err+line
                log_iter.close()
                break
        for line in proc.stderr:
            err = err+line
        t.join()
        proc.communicate()
        out = out[0]
        rv+=1 if proc.returncode!=0 else 0
    except Exception as e:
        print("{}ERROR - Failed to run ({}) - [{}] - [msg: {}]".format(0," ".join(command), 0, e))
        with open(output_err,"a") as ferr:
            ferr.write("{}ERROR - Failed to run ({}) - [{}] - [msg: {}]\n".format(0," ".join(command), 0, e))
            ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
            ferr.write(traceback.format_exc()+"\n")
            ferr.write("--------------------\n\n")
        sys.exit(1)
        rv+=1
    try:
        print("Processing...")
        JDB = json.loads(out)
    except Exception as e:
        print("{}ERROR - Failed to process ({}) - [{}] - [msg: {}]".format(0,"", 0, e))
        print("  {}@RUNNING: {}\n".format(0," ".join(command).replace("(","\\(").replace(")","\\)")))
        with open(output_err,"a") as ferr:
            ferr.write("{}ERROR - Failed to process ({}) - [{}] - [msg: {}]\n".format(0,"", 0, e))
            ferr.write("-------------------- {}\n".format(time.strftime("%Y-%m-%d %H:%M")))
            ferr.write("OUT: {}\n".format(out))
            ferr.write("ERR: {}\n".format(err))
            ferr.write("{}RUNNING: {}\n".format(0," ".join(command).replace("(","\\(").replace(")","\\)")))
            ferr.write(traceback.format_exc()+"\n")
            ferr.write("--------------------\n\n")
        if args.verbose:
            traceback.print_exc()
        sys.exit(1)

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
    if not stream:
        with open(output,"w") as f:
            json.dump(JDB,f,indent="\t")
            # f.write(json.dumps(JDB,indent="\t"))
        print("Done. Written {} [{:.2f}MB]".format(output,float(os.stat(output).st_size)/1048576))
        
        if args.image:
            print("Creating .img database")
            try:
                import libftdb
            except ImportError:
                print("Cannot generate .img database - libftdb not found")
                raise
            libftdb.create_ftdb(JDB,output_img,True)
            print("Done.")

    if mrrs+rv > 0:
        print("WARNING: Encountered some ERRORS!")
    
    if args.forward_output:
        sys.stdout = orig_stdout
        sys.stderr = orig_stderr
        f.close()
    if stream:
        return mrrs+rv, JDB
    else:
        return mrrs+rv
