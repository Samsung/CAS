
from base64 import b64decode, b64encode
from enum import Enum
from functools import lru_cache
import json
from typing import Annotated, Any, Callable, Dict, List, cast
from fastmcp import Context, FastMCP
from fastmcp.exceptions import NotFoundError, ToolError
from libetrace import nfsdbEntry
from pydantic import Field

from client.filtering import CommandFilter
from client.mcp.common import DBType, app_lifespan, get_dbs, DefaultNamespace
from client.mod_base import Module
from client.mod_compilation import RevCompsFor
from client.mod_executables import Binaries, Commands, Execs
from client.mod_misc import CompilerPattern, LinkerPattern
from client.mod_modules import LinkedModules
from client.output_renderers.output import DataTypes
from client.output_renderers.output_json import Renderer
from libcas import CASDatabase


bas_mcp = FastMCP("Build Awareness Service",
                  lifespan=app_lifespan)


@lru_cache(1)
def get_patterns(db: CASDatabase):
    linker_patterns = LinkerPattern(DefaultNamespace(), db, config=db.config).get_data()[0]
    compiler_patterns = CompilerPattern(DefaultNamespace(), db, config=db.config).get_data()[0]
    return linker_patterns | compiler_patterns


@lru_cache(1)
def get_binaries(db: CASDatabase, bin_path):
    return sorted([x for x in db.db.binary_paths() if bin_path in x])


@lru_cache(1)
def get_opened_files(db: CASDatabase, file_path):
    return sorted([x for x in db.opens_paths() if file_path in x])


@lru_cache(1)
def get_ebins(db: CASDatabase, binpath, sort_by_time):
    return sorted(db.get_execs_using_binary(binpath),
                  key=lambda x: (x.etime if sort_by_time else x.eid.pid), reverse=sort_by_time)


@lru_cache(1)
def get_opens(db: CASDatabase, path, opn_filter=False):
    if opn_filter:
        return sorted([x for x in db.opens_paths() if path in x])
    return sorted([x for x in db.opens_paths() if path == x])


@lru_cache(1)
def get_rdeps(db: CASDatabase, path):
    return sorted([f
                   for fp in db.db.rdeps(path, recursive=False)
                   for f in db.linked_modules() if f.path == fp and f.path != path
                   ])

@lru_cache(1)
def get_pattern_names():
    linker_patterns =  LinkerPattern(DefaultNamespace(), None, config=DefaultNamespace()).get_data()[0] # type: ignore
    compiler_patterns = CompilerPattern(DefaultNamespace(), None, config=DefaultNamespace()).get_data()[0] # type: ignore
    return (linker_patterns | compiler_patterns).keys()

@bas_mcp.resource(
                uri="bas://patterns/{pattern_name}",
                name="pattern",
                tags={"bas", "execs", "config"},
                mime_type="application/json",
                description=f"""
                    Configured patterns for categorizing specific executables as linkers or compilers.
                    Available patterns: {get_pattern_names()}
                """)
def pattern(pattern_name: str, ctx: Context):
    """
    Configured patterns for categorizing specific executables as linkers or compilers.
    """
    dbs = get_dbs(ctx)
    nfsdb = dbs.get_nfsdb("")
    patterns = get_patterns(nfsdb)
    return patterns.get(pattern_name, {"error": f"pattern not found, available patterns: {patterns.keys()}"})


@bas_mcp.resource("bas://config", 
                  tags={"bas", "config"},
                  mime_type="application/json",)
def configration(ctx: Context):
    """
    Configuration of the Build Awareness Service.
    """
    dbs = get_dbs(ctx)
    nfsdb = dbs.get_nfsdb("")
    return nfsdb.config.__dict__



@bas_mcp.resource("bas://query/{module}/{params}/page/{page}", name="bas_query")
async def bas_query(ctx: Context, module: str, params: str, page: int, db: str = ""):
    try:
        tool = await bas_mcp.get_tool(module)
    except NotFoundError:
        tools = await bas_mcp.get_tools()
        raise NotFoundError(f"tool not found! available tools: {tools.keys()}")
    arguments = cast(Dict[str, Any], json.loads(b64decode(params)))
    arguments["db"] = db
    arguments["return_type"] = ReturnType.Details   
    result = await tool.run(arguments)
    return result

async def resource_renderer(module: str, page: int, **params: Any):
    tpl = await bas_mcp.get_resource_template("bas_query")
    return await tpl.create_resource("bas://query/{module}/{params}/page/{page}", {
        "module": module,
        "params": b64encode(json.dumps({
            **params
        }).encode()),
        "page": page
    })

class ReturnType(Enum):
    Default = "default"
    Details = "details"
    Resource = "resource"

async def render_output(return_type: ReturnType, data: Any, tool: str, origin: Module | None, output_type: DataTypes, sort_lambda: Callable | None, **params):
    match return_type, origin:
        case ReturnType.Default | ReturnType.Details, None:
            return data
        case ReturnType.Default | ReturnType.Details, _:
            renderer =  Renderer(data, DefaultNamespace(
                count=params.get("count", False),
                entries_per_page=params.get("entries_per_page", 10),
                page=params.get("page", 0),
                details=return_type == ReturnType.Details
            ), origin, output_type, sort_lambda) # type: ignore
            return renderer.render_data()
        case ReturnType.Resource, _:
            renderer =  Renderer(data, DefaultNamespace(
                page=params.get("page", 0),
                details=True,
                count=False,
                entries_per_page=1), origin, output_type, sort_lambda) # type: ignore
            sample = renderer.render_data()
            uri = await resource_renderer(tool, 0, **params)

            return {
                "example": sample,
                "resource": uri
            }

        case _, _:
            raise ToolError(f"Invalid return type, available types: {ReturnType._value2member_map_.keys()}")


queryAnnotations = {
    "readOnlyHint": True,
    "idempotentHint": True,
    "openWorldHint": False,
}

class ExecType(Enum):
    Binaries = "binaries"
    Commands = "commands"

@bas_mcp.tool(
        tags={"bas", "processes"},
        annotations=queryAnnotations,
)
async def execs(ctx: Context,
          type: Annotated[ExecType | None, Field(description="only show executables of a given type - set to None (default) to show all execs")] = None,
          filter: Annotated[str | None, Field(
            description=f"""Filters on files returned, following a format of [keyword=val,[:<keyword=val>]]and/or[]...
            Available keywords:
            - class=<linked/linked_static/linked_shared/linked_exe/compiled/plain>
            - path=<path>
            - type=<sp/re/wc>  (sp - partial match, re - regular expression, wc - wildcard; specified the type for path keywords)
            - access=<r/w/rw>
            - exists=<0/1/2> (1 - file exists, 2 - directory exists)
            - linked=<0/1> (1 - file is a symlink)
            - source_root=<0/1> (speicfy file is in source root)
            - source_type=<c/c++/other>
            - negate=<0/1> (1 - negate the filter)

            If you need a regex pattern for e.g. compilers you can use the `bas://patterns/{{name}}` resource, where the name can be one of {get_pattern_names()}
            """,
            examples=[
                "[path=/usr/bin/*,type=wc]or[exist=1]and[cmd=/bin/bash]",
                "[path=*vmlinux.o,type=wc,exists=1]",
            ])] = None,
          command_filter: Annotated[str | None, Field(
            description="""Filters on commands ran, following a format of [keyword=val,[:<keyword=val>]]and/or[]...
            Available keywords:
            - class=<compiler/linker/command>
            - type=<sp/re/wc> (sp - partial match, re - regular expression, wc - wildcard; specified the type for cwd/bin/cmd keywords)
            - cwd=<path>
            - bin=<path>
            - cmd=<command line filter>
            - ppid=<pid>
            - bin_source_root=<0/1> (specifify binary is in source root)
            - cwd_source_root=<0/1> (specifify working directory is in source root)
            """,
            examples=[
                "[bin=*prebuilts*javac*,type=wc]",
            ])] = None,
            select: Annotated[List[str] | None, Field(
                description="forces return of file paths even if filters disallow them",
                examples=[["/file1", "/file2"]]
            )] = None,
            return_type: Annotated[ReturnType, Field(
                description="Whether to return just a list of files, details (including class, pid, command, etc.) or just a BAS resource URI for to resolve later or pass to other tools"
            )] = ReturnType.Default
          ):
    """
    Returns executables that have been ran during the build; can filter for type (binary or command) or return all execs.
    """
    nfsdb = get_dbs(ctx, DBType.NFSDB)
    match type:
        case ExecType.Binaries:
            module = Binaries(DefaultNamespace(
                filter=filter,
                command_filter=command_filter,
                select=select,
                details=return_type!=ReturnType.Default,
            ), nfsdb, config=nfsdb.config)
        case ExecType.Commands:
            module = Commands(DefaultNamespace(
                filter=filter,
                command_filter=command_filter,
                select=select,
                details=return_type!=ReturnType.Default,
            ), nfsdb, config=nfsdb.config)
        case _:
            module = Execs(DefaultNamespace(
                filter=filter,
                command_filter=command_filter,
                select=select,
                details=return_type!=ReturnType.Default,
            ), nfsdb, config=nfsdb.config)
    data = module.get_data()
    return await render_output(
        return_type,
        data[0],
        "execs",
        module,
        data[1],
        data[2],
        type=type,
        filter=filter,
        command_filter=command_filter,
        select=select,
    )


@bas_mcp.tool(
    tags={"bas", "files"},
    annotations=queryAnnotations,
)
async def linked_modules(ctx: Context,
            filter: Annotated[str | None, Field(
                description="""Filters on files returned, a string following a format of [keyword=val,[:<keyword=val>]]and/or[]...
                Available keywords:
                - class=<linked/linked_static/linked_shared/linked_exe/compiled/plain>
                - path=<path>
                - type=<sp/re/wc>  (sp - partial match, re - regular expression, wc - wildcard; specified the type for path keywords)
                - access=<r/w/rw>
                - exists=<0/1/2> (1 - file exists, 2 - directory exists)
                - linked=<0/1> (1 - file is a symlink)
                - source_root=<0/1> (speicfy file is in source root)
                - source_type=<c/c++/other>
                - negate=<0/1> (1 - negate the filter)
                """,
                examples=[
                    "[path=/usr/bin/*,type=wc]or[exist=1]and[cmd=/bin/bash]",
                    "[path=*vmlinux.o,type=wc,exists=1]",
                ]
            )] = None,
            command_filter: Annotated[str | None, Field(
                description="""Filters on commands ran, a string following a format of [keyword=val,[:<keyword=val>]]and/or[]...
                Available keywords:
                - class=<compiler/linker/command>
                - type=<sp/re/wc> (sp - partial match, re - regular expression, wc - wildcard; specified the type for cwd/bin/cmd keywords)
                - cwd=<path>
                - bin=<path>
                - cmd=<command line filter>
                - ppid=<pid>
                - bin_source_root=<0/1> (specifify binary is in source root)
                - cwd_source_root=<0/1> (specifify working directory is in source root)
                """,
                examples=[
                    "[bin=*prebuilts*javac*,type=wc]",
                ]
            )] = None,
            select: Annotated[List[str] | None, Field(
                description="forces return of file paths even if filters disallow them",
                examples=[["/file1", "/file2"]]
            )] = None,
            return_type: Annotated[ReturnType, Field(
                description="Whether to return just a list of files, details (including class, pid, command, etc.) or just a BAS resource URI for to resolve later or pass to other tools"
            )] = ReturnType.Default,
            commands: Annotated[bool, Field(
                description="Convert file view to commands view"
            )] = False,
            revdeps: Annotated[bool, Field(
                description="Display reverse dependencies of results"
            )] = False,
            path: Annotated[str | None, Field(
                description="Optional shortcut for filtering based on path, adds to the filter keyword"
            )] = None
                         ):
    nfsdb = get_dbs(ctx, DBType.NFSDB)

    if path is not None:
        filter = f"[path=*{path}*,type=wc]" if filter is None else f"{filter}and[path=*{path}*,type=wc]"
    module = LinkedModules(
        DefaultNamespace(
            filter=filter,
            command_filter=command_filter,
            select=select,
            commands=commands,
            revdeps=revdeps,
            details=return_type!=ReturnType.Default,
        ),
        nfsdb,
        DefaultNamespace(),
    )
    data = module.get_data()
    return await render_output(
        return_type,
        data[0],
        "linked_modules",
        module,
        data[1],
        data[2],
        type=type,
        filter=filter,
        command_filter=command_filter,
        select=select,
        commands=commands,
        revdeps=revdeps,
    )
    

def child_renderer(exe: nfsdbEntry, cas_db: CASDatabase | None = None, depth: int = 0):
    return {
        "class": "compiler" if exe.compilation_info is not None
        else "linker" if exe.linked_file is not None
        else "command",
        "pid": exe.eid.pid,
        "idx": exe.eid.index,
        "ppid": exe.parent_eid.pid if hasattr(exe, "parent_eid") else -1,
        "pidx": exe.parent_eid.index if hasattr(exe, "parent_eid") else -1,
        "bin": exe.bpath,
        "cmd": exe.argv,
        "cwd": exe.cwd,
        "pipe_eids": [f'[{o.pid},{o.index}]' for o in exe.pipe_eids],
        "stime": exe.stime if hasattr(exe, "stime") else "",
        "etime": exe.etime,
        "children": [child_renderer(child, cas_db, depth - 1) for child in cas_db.get_eids([(c.pid,) for c in exe.child_cids])] if cas_db is not None and depth > 0 and len(exe.child_cids) > 0 else len(exe.child_cids),
        "wpid": exe.wpid if exe.wpid else "",
        "open_len": len(exe.opens),
    }

@bas_mcp.tool(
    tags={"bas", "files"},
    annotations=queryAnnotations,
)
async def search_files(
    ctx: Context,
    filename: Annotated[str|None, Field(
        description="Filter results by raw filename",
    )] = None,
    command_filter: Annotated[str|None, Field(
        description="""Filters on commands ran, a string following a format of [keyword=val,[:<keyword=val>]]and/or[]...
        Available keywords:
        - class=<compiler/linker/command>
        - type=<sp/re/wc> (sp - partial match, re - regular expression, wc - wildcard; specified the type for cwd/bin/cmd keywords)
        - cwd=<path>
        - bin=<path>
        - cmd=<command line filter>
        - ppid=<pid>
        - bin_source_root=<0/1> (specifify binary is in source root)
        - cwd_source_root=<0/1> (specifify working directory is in source root)
        - negate=<0/1>
        """
    )] = None,
    etime_sort: Annotated[bool, Field(description="sort results based on their etime")] = False,
    entries_per_page: Annotated[int, Field(description="number of results to return", ge=0)] = 10,
    page: Annotated[int, Field(description="0-indexed page to return", ge=0)] = 0,
):
    """
    Search for processes that accessed specified files within the build database using their filenames or other attributes
    """
    nfsdb = get_dbs(ctx, DBType.NFSDB)
    
    # Create command filter if provided
    cmd_filter = None
    if command_filter:
        cmd_filter = CommandFilter(
            command_filter, 
            None, 
            nfsdb.config, 
            nfsdb.source_root
        )
    
    execs = []
    if filename:
        # Get opened files matching filename
        filepaths = get_opened_files(nfsdb, filename)
        if cmd_filter:
            execs = [
                open.parent
                for openedFile in filepaths
                for open in nfsdb.get_opens_of_path(openedFile)
                if cmd_filter.resolve_filters(open.parent)
            ]
        else:
            execs = [
                open.parent
                for openedFile in filepaths
                for open in nfsdb.get_opens_of_path(openedFile)
            ]
    elif command_filter:
        # Filter execs by command filter
        execs = nfsdb.filtered_execs(cast(CommandFilter, cmd_filter).libetrace_filter)
    
    # Handle no results
    if not execs:
        return {"count": 0, "execs": []}
    
    # Apply sorting
    if etime_sort:
        execs = sorted(execs, key=lambda x: x.etime, reverse=etime_sort)
    
    # Apply pagination
    total_count = len(execs)
    total_pages = total_count // entries_per_page
    start_idx = page * entries_per_page
    end_idx = start_idx + entries_per_page
    paginated_execs = execs[start_idx:end_idx]
    
    return {
        "count": total_count,
        "page": page,
        "page_max": total_pages,
        "num_execs": len(paginated_execs),
        "execs":  [child_renderer(exec) for exec in paginated_execs]
    }

@bas_mcp.tool(
    tags={"bas", "processes"},
    annotations=queryAnnotations,
)
async def search_processes(
    ctx: Context,
    pid: Annotated[int | None, Field(description="Process id", ge=0)] = None,
    idx: Annotated[int | None, Field(description="CAS identifier, used to differentiate processes with the same PID. Starts at 0 for the first instance of a given PID.", ge=0)] = None,
    children: Annotated[bool | int, Field(description="Whether to include children in the output and (optionally) how many levels down. If false, only the number of children will be returned")] = False,
    faccess: Annotated[str | None, Field(description="Find processes that accessed the given file")] = None,
):
    """
    Find details of a given process
    """
    nfsdb = get_dbs(ctx, DBType.NFSDB)
    
    # Initialize process list
    processes = []
    
    if pid is not None:
        if idx is not None:
            # Specific process by PID and index
            process = nfsdb.get_exec(pid, idx)
            if process:
                processes.append(process)
        else:
            # All processes with given PID
            processes.extend(nfsdb.get_entries_with_pid(pid))
    
    if faccess:
        # Get all open operations for the file
        opens = nfsdb.get_opens_of_path(faccess)
        for open_op in opens:
            if open_op.parent:
                # Get process from the parent exec
                process = nfsdb.get_exec(
                    open_op.parent.eid.pid, 
                    open_op.parent.eid.index
                )
                if process and process not in processes:
                    processes.append(process)
    
 
    # Render processes using child_renderer
    rendered_processes = []
    for process in processes:
        rendered = child_renderer(process, nfsdb, int(children))
        rendered_processes.append(rendered)
    
    result = {
        "count": len(rendered_processes),
        "processes": rendered_processes
    }
    
    return await render_output(
        ReturnType.Default,
        result,
        "search_processes",
        None,
        DataTypes.commands_data,
        None,
        pid=pid,
        idx=idx,
        children=children,
        faccess=faccess,
    )


@bas_mcp.tool(
    tags={"bas", "files"},
    annotations=queryAnnotations,
)
async def deps_of(
    ctx: Context,  # Context is required for database access
    path: Annotated[str, Field(description="Path to a module")] = "",
    entries_per_page: Annotated[int, Field(description="number of results to return", ge=0)] = 10,
    page: Annotated[int, Field(description="0-indexed page to return", ge=0)] = 0,
):
    """
    Returns list of all files that have been read in process of creating given file.
    """
    nfsdb = get_dbs(ctx, DBType.NFSDB)
    
    # Get direct module dependencies
    deps = nfsdb.db.mdeps(path, direct=True)
    
    linked_paths = nfsdb.linked_module_paths()
    
    # Filter dependencies to exclude self and non-linked modules
    filtered_deps = [
        entry for entry in deps 
        if entry.path != path and entry.path in linked_paths
    ]
    
    # Sort dependencies by path
    sorted_deps = sorted(filtered_deps, key=lambda x: x.path)
    total_count = len(sorted_deps)
    
    # Calculate pagination
    start_index = page * entries_per_page
    end_index = start_index + entries_per_page
    page_deps = sorted_deps[start_index:end_index]
    
    entries_list = []
    for dep in page_deps:
        dep_deps = nfsdb.db.mdeps(dep.path, direct=True)
        dep_deps_filtered = [
            d for d in dep_deps 
            if d.path != dep.path and d.path in linked_paths
        ]
        
        entries_list.append({
            "path": dep.path,
            "num_deps": len(dep_deps_filtered),
            "parent": f"{dep.parent.eid.pid}:{dep.parent.eid.index}" if dep.parent else ""
        })
    
    total_pages = total_count // entries_per_page if total_count else 0
    
    return {
        "count": total_count,
        "page": page,
        "page_max": total_pages,
        "num_entries": len(entries_list),
        "entries": entries_list
    }

@bas_mcp.tool(
    tags={"bas", "files"},
    annotations=queryAnnotations,
)
async def revdeps_of(
    ctx: Context,  # Context is required for database access
    path: Annotated[str, Field(description="Path to a module")] = "",
    entries_per_page: Annotated[int, Field(description="number of results to return", ge=0)] = 10,
    page: Annotated[int, Field(description="0-indexed page to return", ge=0)] = 0,
):
    """
    Returns all files that depend on given file.
    """
    nfsdb = get_dbs(ctx, DBType.NFSDB)
    
    # Get reverse dependencies
    entries = get_rdeps(nfsdb, path)
    total_count = len(entries)
    
    # Calculate pagination
    start_index = page * entries_per_page
    end_index = start_index + entries_per_page
    page_entries = entries[start_index:end_index]
    
    result_entries = []
    for entry in page_entries:
        rdeps_count = len(get_rdeps(nfsdb, entry.path))
        
        result_entries.append({
            "path": entry.path,
            "num_deps": rdeps_count,
            "class": entry.path in nfsdb.linked_module_paths(),
            "parent": str(entry.parent.eid.pid) if entry.parent else ""
        })
    
    total_pages = total_count // entries_per_page if total_count else 0
    
    return {
        "count": total_count,
        "page": page,
        "page_max": total_pages,
        "num_entries": len(result_entries),
        "entries": result_entries
    }

@bas_mcp.tool(
    tags={"bas", "files"},
    annotations=queryAnnotations,
)
async def revcomps(
    ctx: Context,
    path: Annotated[str, Field(description="Path to a file to find reverse compilations for")] = "",
    filter: Annotated[str | None, Field(
        description="""Filters on files returned, a string following a format of [keyword=val,[:<keyword=val>]]and/or[]...
        Available keywords:
        - class=<linked/linked_static/linked_shared/linked_exe/compiled/plain>
        - path=<path>
        - type=<sp/re/wc>  (sp - partial match, re - regular expression, wc - wildcard; specified the type for path keywords)
        - access=<r/w/rw>
        - exists=<0/1/2> (1 - file exists, 2 - directory exists)
        - linked=<0/1> (1 - file is a symlink)
        - source_root=<0/1> (speicfy file is in source root)
        - source_type=<c/c++/other>
        - negate=<0/1> (1 - negate the filter)
        """,
        examples=[
            "[path=/usr/bin/*,type=wc]or[exist=1]and[cmd=/bin/bash]",
            "[path=*vmlinux.o,type=wc,exists=1]",
        ]
    )] = None,
    command_filter: Annotated[str | None, Field(
        description="""Filters on commands ran, a string following a format of [keyword=val,[:<keyword=val>]]and/or[]...
        Available keywords:
        - class=<compiler/linker/command>
        - type=<sp/re/wc> (sp - partial match, re - regular expression, wc - wildcard; specified the type for cwd/bin/cmd keywords)
        - cwd=<path>
        - bin=<path>
        - cmd=<command line filter>
        - ppid=<pid>
        - bin_source_root=<0/1> (specifify binary is in source root)
        - cwd_source_root=<0/1> (specifify working directory is in source root)
        """,
        examples=[
            "[bin=*prebuilts*javac*,type=wc]",
        ]
    )] = None,
    select: Annotated[List[str] | None, Field(
        description="forces return of file paths even if filters disallow them",
        examples=[["/file1", "/file2"]]
    )] = None,
    return_type: Annotated[ReturnType, Field(
        description="Whether to return just a list of files, details (including class, pid, command, etc.) or just a BAS resource URI for to resolve later or pass to other tools"
    )] = ReturnType.Default,
    commands: Annotated[bool, Field(
        description="Convert file view to commands view"
    )] = False,
    revdeps: Annotated[bool, Field(
        description="Display reverse dependencies of results"
    )] = False,
    match: Annotated[str | None, Field(
        description="Pattern matching for source files"
    )] = None,
    cdb: Annotated[bool, Field(
        description="Output in compile_commands.json format"
    )] = False,
):
    """
    Returns sources list used in compilation that in some point referenced given file.
    """
    nfsdb = get_dbs(ctx, DBType.NFSDB)
    module = RevCompsFor(
        DefaultNamespace(
            path=path,
            filter=filter,
            command_filter=command_filter,
            select=select,
            commands=commands,
            revdeps=revdeps,
            match=match,
            cdb=cdb,
            details=return_type != ReturnType.Default,
        ),
        nfsdb,
        config=nfsdb.config
    )
    
    data = module.get_data()
    return await render_output(
        return_type,
        data[0],
        "revcomps",
        module,
        data[1],
        data[2],
        path=path,
        filter=filter,
        command_filter=command_filter,
        select=select,
        commands=commands,
        revdeps=revdeps,
        match=match,
        cdb=cdb,
    )