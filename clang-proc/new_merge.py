#!/usr/bin/env python

import multiprocessing

# Following is assumed about provided databases

# db1 types have unique hashes and consecutive ids
# db1 globals have unique hashes and consecutive ids
# db1 funcs have unique hashes
# db1 funcs may have duplicate declhashes in some cases
# db1 funcdecls have unique declhashes
# db1 ufuncs have unique names
# db1 funcs, funcdecls and ufuncs together hav consecutive ids

# db2 types may have duplicate hashes
# db2 globals have unique hashes
# db2 funcs have unique hashes
# db2 funcs may have duplicate declhashes
# db2 funcdecls have unique declhashes(with funcs as well)
# db2 ufuncs have unique names


# Objets from db2 are merged to db1 in the following way

# new types are added with consecutive type ids
# duplicate types are discarded

# new globals are added with consecutive global ids
# old globals with lower deftypes are replaced with new ones
# (skipped)global decls with record forward type are discared when record type decl are found
# duplicate global definitions cause an error

# new ufuncs are added with consecutive func ids
# duplicate ufuncs are discarded

# new funcdecls are added with consecutive func ids
# duplicate funcdecls are discarded
# funcdecls with external funcs with the same declhash are discarded

# new funcs are added with consecutive func ids
# duplicate declhashes with other funcs are allowed in some cases, otherwise cause an error
# old decls with the same declhash as func are removed and func takes their id
# weak function definitions are not referenced if non weak versions are present [REFACTOR]
# duplicate funcs are discarded

# For now c++ is not supported

# TODO: maybe optimize file indexing (only update necessary objects)
# TODO: handle  various missing* fields in main db
def merge_json_ast(db1, db2,
                   quiet=False,
                   debug=False,
                   verbose=False,
                   file_logs=None,
                   test=False,
                   chunk=None,
                   threadid=None,
                   exit_on_error=False,
                   job_count=multiprocessing.cpu_count(),
                   copy_jdba=True):
    if db1 is None:
        # create empty database
        db1 = {
            "types": [],
            "globals": [],
            "funcs": [],
            "funcdecls": [],
            "unresolvedfuncs": [],
            "typen": 0,
            "globaln": 0,
            "funcn": 0,
            "funcdecln": 0,
            "unresolvedfuncn": 0,
            "sourcen": 0,
            "sources": [],
            "directory": db2["directory"],
        }
        # TODO: add missing if necessary

    # file_id offset
    next_file_id = db1["sourcen"]

    # next unused id for each category
    nexttypeid = db1["typen"]
    nextvarid = db1["globaln"]
    nextfuncid = db1["funcn"]+db1["funcdecln"]+db1["unresolvedfuncn"]

    # only for ids in db2
    # we will iterate through db2 and replace all ids in those maps
    typeid_remap = {}
    varid_remap = {}
    funcid_remap = {}

    # track added object
    added_types = []
    added_vars = []
    added_funcs = []
    added_fdecls = []
    added_ufuncs = []

    # track objcts to remove
    removed_vars = []
    removed_fdecls = []

# [REFACTOR] this section may be removed for single merging thread version
# idea is that structures defined below will persist thoughout the generation process
    # generate hashmaps for db1
    def get_hashmap(db_key, db_val_key, err_msg):
        ret_map = {}
        for item in db1[db_key]:
            h = item[db_val_key]
            if h not in ret_map:
                ret_map[h] = item
            else:
                assert False, f"{err_msg}: {h}"
        return ret_map

    typemap = get_hashmap('types', 'hash', 'Duplicate type hash in database')
    varmap = get_hashmap('globals', 'hash', 'Duplicate global hash in database')
    ufuncmap = get_hashmap('unresolvedfuncs', 'name', 'Duplicate unused function name in database')
    fdeclmap = get_hashmap('funcdecls', 'declhash', 'Duplicate function declaration hash in database')

    funcmap = {}
    dfuncmap = {}
    for func in db1["funcs"]:
        h = func["hash"]
        if h not in funcmap:
            funcmap[h] = func
        else:
            # TODO: implement check for allowed cases
            assert False, f"Duplicate function hash in database {h}"
        if func["linkage"] != "internal":
            dh = func["declhash"]
            if dh not in dfuncmap:
                dfuncmap[dh] = func
            else:
                prev = dfuncmap[dh]
                if "weak" in prev["attributes"]:
                    # reference non weak version if present
                    # consistent with previous merge version
                    # TODO: impement merging weak functions instead
                    dfuncmap[h] = func
                else:
                    pass
                        # TODO: implement check for disallowed cases
# [REFACTOR] ends here

# TODO: maybe verify assumptions about db2 data before merging

    # types
    # add new types
    # discard duplicate types
    for i, tp in enumerate(db2["types"]):
        tp["fid"] = tp["fid"] + next_file_id
        h = tp["hash"]
        if h not in typemap:
            # add new type to database
            typemap[h] = tp
            added_types.append(i)
            typeid_remap[tp["id"]] = nexttypeid
            tp["id"] = nexttypeid
            nexttypeid += 1
        else:
            # merge new types with old types
            # seems the cases where additional merge is needed are deprecated
            # self merge (some types are duplicated in db2 )
            # TODO: add check for disallowed type classes if needed
            # discard duplicate types
            prev = typemap[h]
            typeid_remap[tp["id"]] = prev["id"]
            # update refcount
            typemap[h]["refcount"] += 1
            # merge usedrefs
            if "usedrefs" in tp:
                prev["usedrefs"] = [x if max(a,b)>=0 else -1 for a,b,x in zip(tp["usedrefs"],prev["usedrefs"],prev["refs"])]
            if "useddef" in tp:
                prev["useddef"] = [ x if a != -1 else y for a,x,y in zip(tp["usedrefs"],tp["useddef"],prev["useddef"])]


    # vars
    # add new vars
    # replace old var when new has higher deftype
    # replace old var deftype=0 when new has full record type
    for i, var in enumerate(db2["globals"]):
        var["fid"] = var["fid"] + next_file_id
        h = var["hash"]
        if h not in varmap:
            # add new global to database
            varmap[h] = var
            added_vars.append(i)
            varid_remap[var["id"]] = nextvarid
            var["id"] = nextvarid
            nextvarid += 1
        else:
            # merge new globals with old globals
            # some vars from db1 needs to be replaced
            # no self merge allowed
            prev = varmap[h]
            varid_remap[var["id"]] = prev["id"]
            # check duplicate definition
            if prev["deftype"] == var["deftype"] == 2:
                pass
                # ignore duplicates until modules are properly handles
                # assert False, "Multiple definitions of extern variable %s"%(var["name"])
            # replace delcaration with definition
            if prev["deftype"] < var["deftype"]:
                var["id"] = prev["id"]
                removed_vars.append(prev)
                added_vars.append(i)
                varmap[h] = var
            # TODO: update record forward to full record types if possible
            # ignored for now, it is costly and not really necessary


    # ufuncs
    # add new ufuncs
    # discard duplicare ufuncs
    for i, ufunc in enumerate(db2["unresolvedfuncs"]):
        n = ufunc["name"]
        if n not in ufuncmap:
            # add new ufunc to database
            ufuncmap[n] = ufunc
            added_ufuncs.append(i)
            funcid_remap[ufunc["id"]] = nextfuncid
            ufunc["id"] = nextfuncid
            nextfuncid += 1
        else:
            # discard duplicate ufunc
            # no self merge allowed
            funcid_remap[ufunc["id"]] = ufuncmap[n]["id"]


    # fdecls
    # add new fdecls
    # discard when definition in db
    # discard duplicate fdecls
    for i, fdecl in enumerate(db2["funcdecls"]):
        fdecl["fid"] = fdecl["fid"] + next_file_id
        dh = fdecl["declhash"]
        if dh not in fdeclmap:
            if dh not in dfuncmap:
                # add new fdecl to database
                fdeclmap[dh] = fdecl
                added_fdecls.append(i)
                funcid_remap[fdecl["id"]] = nextfuncid
                fdecl["id"] = nextfuncid
                nextfuncid += 1
            else:
                # discard defined decl
                funcid_remap[fdecl["id"]] = dfuncmap[dh]["id"]
        else:
            # discard duplicate decl
            # no self merge allowed
            funcid_remap[fdecl["id"]] = fdeclmap[dh]["id"]
            # update refcount
            fdeclmap[dh]["refcount"] += 1


    # funcs
    # add new funcs
    # replace decl defined by this func
    # discard duplicate funcs
    for i, func in enumerate(db2["funcs"]):
        func["fid"] = func["fid"] + next_file_id
        func["fids"] = [file_id +next_file_id for file_id in func["fids"]]
        h = func["hash"]
        dh = func["declhash"]
        if h not in funcmap:
            # TODO: check for declhash conflicts with functions in db
            # add new function
            funcmap[h] = func
            added_funcs.append(i)
            if dh not in fdeclmap or func["linkage"] == "internal":
                # new id
                funcid_remap[func["id"]] = nextfuncid
                func["id"] = nextfuncid
                nextfuncid+=1
            else:
                # replace decl
                funcid_remap[func["id"]] = fdeclmap[dh]["id"]
                func["id"] = fdeclmap[dh]["id"]
                removed_fdecls.append(fdeclmap[dh])
                del fdeclmap[dh]
        else:
            # discard duplicate func
            funcid_remap[func["id"]] = funcmap[h]["id"]
            funcmap[h]["fids"].extend(func["fids"])
            # update refcount
            funcmap[h]["refcount"] += 1


    # Update ids of objects added to db1
    for i in added_types:
        tp = db2["types"][i]
        # tp["id"] already updated
        tp["refs"] = [typeid_remap[tp_id] for tp_id in tp["refs"]]
        if "usedrefs" in tp:
            tp["usedrefs"] = [typeid_remap[tp_id] if tp_id!=-1 else -1 for tp_id in tp["usedrefs"]]
        if "globalrefs" in tp:
            tp["globalrefs"] = [varid_remap[var_id] for var_id in tp["globalrefs"]]
        if "outerfnid" in tp:
            tp["outerfnid"] = funcid_remap[tp["outerfnid"]]
        if "attrrefs" in tp:
            tp["attrrefs"] = [typeid_remap[tp_id] for tp_id in tp["attrrefs"]]
        if "funrefs" in tp:
            tp["funrefs"] = [funcid_remap[func_id] for func_id in tp["funrefs"]]
        if "methods" in tp:
            tp["methods"] = [funcid_remap[func_id] for func_id in tp["methods"]]
    
    for i in added_vars:
        var = db2["globals"][i]
        # var["id"] already updated
        var["type"] = typeid_remap[var["type"]]
        var["refs"] = [typeid_remap[tp_id] for tp_id in var["refs"]]
        var["globalrefs"] = [varid_remap[var_id] for var_id in var["globalrefs"]]
        var["funrefs"] = [funcid_remap[func_id] for func_id in var["funrefs"]]

    for i in added_ufuncs:
        ufunc = db2["unresolvedfuncs"][i]
        # ufunc["id"] already updated

    for i in added_fdecls:
        fdecl = db2["funcdecls"][i]
        # fdecl["id"] already updated
        fdecl["types"] = [typeid_remap[tp_id] for tp_id in fdecl["types"]]

    for i in added_funcs:
        func = db2["funcs"][i]
        # func["id"] already updated
        func["types"] = [typeid_remap[tp_id] for tp_id in func["types"]]
        func["refs"] = [typeid_remap[tp_id] for tp_id in func["refs"]]
        func["globalrefs"] = [varid_remap[var_id] for var_id in func["globalrefs"]]
        func["funrefs"] = [funcid_remap[func_id] for func_id in func["funrefs"]]
        func["calls"] = [funcid_remap[func_id] for func_id in func["calls"]]
        for refcall in func["refcalls"]:
            if len(refcall) == 1:
                refcall[0] = typeid_remap[refcall[0]]
            elif len(refcall) ==3:
                refcall[0] = typeid_remap[refcall[0]]
                refcall[1] = typeid_remap[refcall[1]]
        for callref in func["callrefs"]:
            for arg in callref:
                if arg["type"] == "global":
                    arg["id"] = varid_remap[arg["id"]]
                elif arg["type"] == "function":
                    arg["id"] = funcid_remap[arg["id"]]
        for refcallref in func["refcallrefs"]:
            for arg in refcallref:
                if arg["type"] == "global":
                    arg["id"] = varid_remap[arg["id"]]
                elif arg["type"] == "function":
                    arg["id"] = funcid_remap[arg["id"]]
        for local in func["locals"]:
            local["type"] = typeid_remap[local["type"]]
        for ifstmt in func["ifs"]:
            for ref in ifstmt["refs"]:
                if ref["type"] == "global":
                    ref["id"] = varid_remap[ref["id"]]
                elif ref["type"] == "function":
                    ref["id"] == funcid_remap[ref["id"]]
        for deref in func["derefs"]:
            if "type" in deref:
                deref["type"] = [typeid_remap[tp_id] for tp_id in deref["type"]]
            for ref in deref["offsetrefs"]:
                if ref["kind"] == "global":
                    ref["id"] = varid_remap[ref["id"]]
                elif ref["kind"] == "function":
                    ref["id"] = funcid_remap[ref["id"]]
                if "cast" in ref:
                    ref["cast"] = typeid_remap[ref["cast"]]

    # Add and remove objects
    db1["globals"] = [var for var in db1["globals"] if var not in removed_vars]
    db1["funcdecls"] = [fdecl for fdecl in db1["funcdecls"] if fdecl not in removed_fdecls]

    db1["types"].extend([db2["types"][i] for i in added_types])
    db1["globals"].extend([db2["globals"][i] for i in added_vars])
    db1["funcs"].extend([db2["funcs"][i] for i in added_funcs])
    db1["funcdecls"].extend([db2["funcdecls"][i] for i in added_fdecls])
    db1["unresolvedfuncs"].extend([db2["unresolvedfuncs"][i] for i in added_ufuncs])

    db1["sources"].extend([{list(s.keys())[0]:list(s.values())[0]+next_file_id} for s in db2["sources"]])

    # Update sizes
    db1["typen"] = len(db1["types"])
    db1["globaln"] = len(db1["globals"])
    db1["funcn"] = len(db1["funcs"])
    db1["funcdecln"] = len(db1["funcdecls"])
    db1["unresolvedfuncn"] = len(db1["unresolvedfuncs"])
    db1["sourcen"] = len(db1["sources"])
    print(f"Summary: types: {db1['typen']}, globals: {db1['globaln']}, funcs: {db1['funcn']}, funcdecls: {db1['funcdecln']}, unresolvedfuncs {db1['unresolvedfuncn']}")

    # Finished merging
    return db1
