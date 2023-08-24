import os
import shutil
import json
import re
import sys

class APGN(object):
    
    def __init__(self,nfsdb):
        self.nfsdb = nfsdb
        self.cmap = {}
        h = re.compile("^/dev/.*")
        for e in nfsdb:
            if e.has_compilations():
                for cf in e.compilation_info.file_paths:
                    if not h.match(cf):
                        if cf not in self.cmap:
                            self.cmap[cf] = [e]
                        else:
                            self.cmap[cf].append(e)
    
    def path_under_root(self,path,root):
        path_components = iter(path.split(os.sep)[1:])
        current_path = ""
        original_root = ""
        for pc in path_components:
            current_path+=os.sep+pc
            if os.path.islink(current_path):
                current_path = os.path.normpath(os.readlink(current_path))
            original_root+=os.sep+pc
            if current_path==root:
                return os.sep.join([u for u in path_components]), original_root
        return None,None

    def generate_project_description_files(self,
            project_files_list,
            module_name,
            dep_pids,
            source_root,
            outdir,
            verbose=False,
            debug=False,
            quiet=False):

        pflst = list()
        lnklst = list()
        symlst = list()
        real_root = os.path.realpath(source_root)
        nroot = os.path.join(os.path.realpath(outdir),source_root.split(os.sep)[-1])
        for fn in project_files_list:
            if not self.nfsdb.path_regular(fn) and not self.nfsdb.path_symlinked(fn):
                continue
            if self.nfsdb.path_symlinked(fn):
                # Save the list of original paths accessed through the symlinks (need to be copied as well)
                symlst+=[os.path.normpath(x) for x in self.nfsdb.symlink_paths(fn) if os.path.exists(x)]
            pfn,original_root = self.path_under_root(fn,real_root)
            if pfn:
                if os.path.exists(fn):
                    if len(pfn)>0:
                        pflst.append( (pfn,original_root) )

        symlst = list(set(symlst))
        for fn in symlst:
            lnk = os.readlink(fn)
            if os.path.isabs(lnk):
                # Resolve the link to the new location
                lnk.replace(source_root,nroot)
            pfn,original_root = self.path_under_root(fn,real_root)
            if pfn:
                if len(pfn)>0:
                    lnklst.append( (pfn,original_root,lnk) )

        compilation_executions = list()
        compiled_files = set()
        for pf in pflst:
            pfpath = os.path.join(pf[1],pf[0])
            if pfpath in self.cmap:
                compilation_executions+=self.cmap[pfpath]
                compiled_files.add(pfpath)

        # Extract all referenced files by all compilation executions
        all_compiler_references = set()
        for ce in compilation_executions:
            ce_all = self.nfsdb[(ce.eid.pid,)]
            for this_ce in ce_all:
                all_compiler_references|=this_ce.openpaths_with_children

        # Add special macro that is only available at IDE
        # X.append(("__BAS__IDE_PROJECT_GENERATOR__",""))

        def json_command(cmd):
            return " ".join([x.rstrip().replace("\\","\\\\").replace("\"","\\\"").replace(" ","\\ ") for x in cmd])

        compile_commands=list()
        for pf in pflst:
            pfpath = os.path.join(pf[1],pf[0])
            if pfpath in compiled_files:
                eL = self.cmap[pfpath]
                for e in eL:
                    compile_commands.append(
                        "{\"directory\":%s,\"command\":%s,\"file\":%s}"%(
                            json.dumps(e.cwd.replace(source_root,nroot)),
                            json.dumps(json_command(e.argv).replace(source_root,nroot)),
                            json.dumps(pfpath.replace(source_root,nroot))
                        )
                    )

        if not os.path.exists(outdir):
            os.makedirs(outdir)

        with open(os.path.join(outdir,"compile_commands.json"),"w") as f:
            f.write(json.dumps(json.loads("[%s]"%",".join(compile_commands)), indent=4, sort_keys=False))
        print ("Written compilation database")

        # Write project files

        # Copy the project files
        sys.stdout.write("Copying project files... 0/%d"%(len(pflst)+len(lnklst)))
        sys.stdout.flush()
        cnt=0
        for pfn,pfroot in pflst:
            pfpath = os.path.join(pfroot,pfn)
            dst_path = os.path.join(nroot,pfn)
            if not os.path.exists(os.path.dirname(dst_path)):
                os.makedirs(os.path.dirname(dst_path))
            shutil.copyfile(pfpath,dst_path)
            cnt+=1
            sys.stdout.write("\rCopying project files... %d/%d"%(cnt,len(pflst)+len(lnklst)))
        sys.stdout.write("\n")
        sys.stdout.flush()
        # Now handle the links
        for pfn,pfroot,lnk in lnklst:
            dst_path = os.path.join(nroot,pfn)
            if not os.path.exists(os.path.dirname(dst_path)):
                os.makedirs(os.path.dirname(dst_path))
            if os.path.exists(dst_path):
                os.remove(dst_path)
            os.symlink(lnk, dst_path)
            cnt+=1
            sys.stdout.write("\rCopying project files... %d/%d"%(cnt,len(pflst)+len(lnklst)))
        print()