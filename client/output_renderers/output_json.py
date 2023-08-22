import json
from typing import Dict
from client.output_renderers.output import OutputRenderer
from client.misc import fix_cmd, access_from_code, get_file_info
import libetrace
import libcas


class Renderer(OutputRenderer):
    """
    Plain output - targeted for using in API calls.
    """

    help = 'Prints output in json format (generated from output_renderers dir)'
    default_entries_count = 1000

    entries_format = '''{{
    "count": {count},
    "page": {page},
    "num_entries": {num_entries},
    "entries": [{entries}]
}}'''
    entries_match_format = '''{{
    "count": {count},
    "page": {page},
    "num_entries": {num_entries},
    "entries": [{entries}],
    "match": [{match}]
}}'''

    @staticmethod
    def append_args(parser):
        pass

    def count_renderer(self):
        return json.dumps({"count": self.num_entries})

    def formatter(self, format_func=None):
        if self.count > 0:
            if format_func is not None:
                return self.entries_format.format(
                    count=self.count,
                    page=0 if not self.args.page else self.args.page,
                    num_entries=self.num_entries,
                    entries="\n"+",\n".join([format_func(row) for row in self.data])+"\n    "
                    )
            if isinstance(self.data, libetrace.nfsdbFilteredOpensPathsIter):
                return self.entries_format.format(
                    count=self.count,
                    page=0 if not self.args.page else self.args.page,
                    num_entries=self.num_entries,
                    entries="\n"+",\n".join(['        "'+self.origin_module.get_path(row)+'"' for row in self.data])+"\n    "
                    )
            if isinstance(self.data, libetrace.nfsdbFilteredOpensIter) or isinstance(self.data, libetrace.nfsdbOpensIter):
                return self.entries_format.format(
                    count=self.count,
                    page=0 if not self.args.page else self.args.page,
                    num_entries=self.num_entries,
                    entries="\n"+",\n".join([self._file_entry_format(row) for row in self.data])+"\n    "
                )
            if isinstance(self.data, libetrace.nfsdbFilteredCommandsIter):
                if self.args.generate:
                    if self.args.makefile:
                        return self.makefile_data_renderer()
                    else:
                        return "[\n" + ',\n'.join([(self._command_generate_openrefs_entry_format(row) if self.args.openrefs else self._command_generate_entry_format(row)) for row in self.data]) + "\n]"
                else:
                    return self.entries_format.format(
                        count=self.count,
                        page=0 if not self.args.page else self.args.page,
                        num_entries=self.num_entries,
                        entries="\n"+",\n".join([(self._command_entry_format_openrefs(row) if self.args.openrefs else self._command_entry_format(row)) for row in self.data])+"\n    "
                    )
            # Assign formatter according to output type
            if isinstance(self.data[0], str):
                if self.data[0][0] == '"' and self.data[0][-1] == '"':
                    fmt = '        ', ''
                else:
                    fmt = '        "', '"'
                return self.entries_format.format(
                    count=self.count,
                    page=0 if not self.args.page else self.args.page,
                    num_entries=self.num_entries,
                    entries="\n"+",\n".join([fmt[0]+self.origin_module.get_path(row)+fmt[1] for row in self.data])+"\n    "
                    )
            elif isinstance(self.data[0], libetrace.nfsdbEntryOpenfile):
                return self.entries_format.format(
                    count=self.count,
                    page=0 if not self.args.page else self.args.page,
                    num_entries=self.num_entries,
                    entries="\n"+",\n".join([self._file_entry_format(row) for row in self.data])+"\n    "
                )
            elif isinstance(self.data[0], libetrace.nfsdbEntry):
                if self.args.generate:
                    if self.args.makefile:
                        return self.makefile_data_renderer()
                    else:
                        return "[\n" + ',\n'.join([(self._command_generate_openrefs_entry_format(row) if self.args.openrefs else self._command_generate_entry_format(row)) for row in self.data]) + "\n]"
                else:
                    if self.args.proc_tree:
                        return {
                                "count": self.count,
                                "page": 0 if not self.args.page else self.args.page,
                                "page_max": int(self.num_entries / self.args.entries_per_page),
                                "entries_per_page": self.args.entries_per_page,
                                "num_entries": self.num_entries,
                                "entries": [self._command_entry_format_proc_tree(row) for row in self.data]
                        }
                    return self.entries_format.format(
                        count=self.count,
                        page=0 if not self.args.page else self.args.page,
                        num_entries=self.num_entries,
                        entries="\n"+",\n".join([self._command_entry_format(row) for row in self.data])+"\n    "
                    )
            else:
                return self.entries_format.format(
                    count=self.count,
                    page=0 if not self.args.page else self.args.page,
                    num_entries=self.num_entries,
                    entries="\n"+",\n".join([row for row in self.data])+"\n    "
                    )
        else:
            if self.args.generate:
                if self.args.makefile:
                    return None
                else:
                    return self.entries_format.format(
                        count=self.count,
                        page=0 if not self.args.page else self.args.page,
                        num_entries=self.num_entries,
                        entries=",\n".join([])
                    )
            return self.entries_format.format(
                count=self.count,
                page=0 if not self.args.page else self.args.page,
                num_entries=self.num_entries,
                entries=",\n".join([])
                )

    def linked_data_renderer(self):
        return self.formatter()

    def file_data_renderer(self):
        return self.formatter()

    def process_data_renderer(self):
        return self.formatter(self._process_entry_format)

    def binary_data_renderer(self):
        return self.formatter()

    def commands_data_renderer(self):
        return self.formatter()

    def compiled_data_renderer(self):
        return self.formatter()

    def compilation_info_data_renderer(self):
        return self.formatter(self._compilation_info_entry_format)

    # def _str_entry_format(self, row, escaped):
    #     if escaped:
    #         return '        {}'.format(self.origin_module.get_path(row) if self.args.relative else row)
    #     else:
    #         return '        "{}"'.format(self.origin_module.get_path(row) if self.args.relative else row)
    def _command_entry_format_proc_tree(self, row):
        return {
                "class": "compiler" if row.compilation_info is not None 
                    else "linker" if row.linked_file is not None
                    else "command",
                "pid": row.eid.pid,
                "idx": row.eid.index,
                "ppid": row.parent_eid.pid if hasattr(row, "parent_eid") else -1,
                "pidx": row.parent_eid.index if hasattr(row, "parent_eid") else -1,
                "bin": row.bpath,
                "cmd": row.argv,
                "cwd": row.cwd,
                "pipe_eids": [f'[{o.pid},{o.index}]' for o in row.pipe_eids],
                "etime": row.etime,
                "children": len(self.origin_module.nfsdb.get_entries_with_pids([(c.pid,) for c in row.child_cids])),
                "wpid": row.wpid if row.wpid else "",
                "open_len": len(row.opens),
            }
    
    def _command_entry_format(self, row: libetrace.nfsdbEntry):
        return '''        {{
            "class": "{cls}",
            "pid": {pid},
            "idx": {idx},
            "ppid": {ppid},
            "pidx": {pidx},
            "bin": "{bin}",
            "cwd": "{cwd}",
            "command": {command}{linked}{compiled}{objects}
        }}'''.format(
            cls="compiler" if row.compilation_info is not None else "linker" if row.linked_file is not None else "command",
            pid=row.eid.pid,
            idx=row.eid.index,
            ppid=row.parent_eid.pid,
            pidx=row.parent_eid.index,
            bin=row.bpath,
            cwd=row.cwd,
            command=fix_cmd(row.argv, join=not ("raw_command" in self.args and self.args.raw_command)),
            linked="" if row.linked_file is None else ',\n            "linked": "{filename}"'.format(filename=row.linked_file.path),
            compiled="" if row.compilation_info is None or not len(row.compilation_info.files) > 0 else ',\n            "compiled": {{\n                "{filename}": "{f_type}"\n            }}'.format(
                filename=row.compilation_info.files[0].path, f_type={1: "c", 2: "c++", 3: "other"}[row.compilation_info.type]).replace("'", '"'),
            objects="" if not (row.compilation_info is not None and len(row.compilation_info.object_paths) > 0) else 
                ',\n            "objects": [ \n                {objects}\n            ]'.format(objects=",\n                ".join(['"{}"'.format(o) for o in row.compilation_info.object_paths])).replace("'", '"'),
    )

    def _command_entry_format_openrefs(self, row:libetrace.nfsdbEntry):
        return '''        {{
            "class": "{cls}",
            "pid": {pid},
            "idx": {idx},
            "ppid": {ppid},
            "pidx": {pidx},
            "bin": "{bin}",
            "cwd": "{cwd}",
            "command": {command}{linked}{compiled}{objects},
            "openfiles": [{refs}]
        }}'''.format(
            cls="compiler" if row.compilation_info is not None else "linker" if row.linked_file is not None else "command",
            pid=row.eid.pid,
            idx=row.eid.index,
            ppid=row.parent_eid.pid,
            pidx=row.parent_eid.index,
            bin=row.bpath,
            cwd=row.cwd,
            command=fix_cmd(row.argv, join=not ("raw_command" in self.args and self.args.raw_command)),
            linked="" if row.linked_file is None else ',\n            "linked": "{filename}"'.format(filename=row.linked_file.path),
            compiled="" if row.compilation_info is None or not len(row.compilation_info.files) > 0 else ',\n            "compiled": {{\n                "{filename}": "{f_type}"\n            }}'.format(
                filename=row.compilation_info.files[0].path, f_type={1: "c", 2: "c++", 3: "other"}[row.compilation_info.type]).replace("'", '"'),
            objects="" if not (row.compilation_info is not None and len(row.compilation_info.objects) > 0) else 
                ',\n            "objects": [ \n                {objects}\n            ]'.format(objects=",\n                ".join(['"{}"'.format(o.path) for o in row.compilation_info.objects])).replace("'", '"'),
            refs=("" if not row.opens else '\n                '+',\n                '.join(['"{}"'.format(o.path) for o in row.opens]) + "\n            ")
    )    

    def _compilation_info_entry_format(self, opn:libetrace.nfsdbEntryOpenfile):
        row:libetrace.nfsdbEntry = opn.opaque
        return """        {{
            "command": {{
                "pid": {pid},
                "idx": {idx},
                "ppid": {ppid},
                "pidx": {pidx},
                "bin": "{bin}",
                "cwd": "{cwd}",
                "command": {command}
            }},
            "source_type": "{src_type}",
            "compiled_files": {compiled_files},
            "include_paths": {include_paths},
            "headers": {include_files},
            "defs": {defs},
            "undefs": {undefs},
            "deps": {deps}
        }}""".format(
            pid=row.eid.pid,
            idx=row.eid.index,
            ppid=row.parent_eid.pid,
            pidx=row.parent_eid.index,
            bin=row.bpath,
            cwd=row.cwd,
            command=fix_cmd(row.argv),
            src_type={1: "c", 2: "c++", 3: "other"}[row.compilation_info.type],
            compiled_files='[{}{}\n            ]'.format("\n" if len(row.compilation_info.files) > 0 else "", ",\n".join(['                "' + d + '"' for d in sorted(row.compilation_info.file_paths)])),
            include_files='[{}{}\n            ]'.format("\n" if len(row.compilation_info.headers) > 0 else "", ",\n".join(['                "' + d + '"' for d in sorted(row.compilation_info.header_paths)])),
            include_paths='[{}{}\n            ]'.format("\n" if len(row.compilation_info.ipaths) > 0 else "", ",\n".join(['                "' + d + '"' for d in sorted(row.compilation_info.ipaths)])),
            defs='[{}{}\n            ]'.format("\n" if len(row.compilation_info.defs) > 0 else "", 
                ",\n".join(['                {{\n                    "name": "{}",\n                    "value": {}\n                }}'.format(d[0], json.dumps(d[1])) for d in sorted(row.compilation_info.defs)])),
            undefs='[{}{}\n            ]'.format("\n" if len(row.compilation_info.undefs) > 0 else "", 
                ",\n".join(['                {{\n                    "name": "{}",\n                    "value": {}\n                }}'.format(d[0], json.dumps(d[1])) for d in sorted(row.compilation_info.undefs)])),
            deps=[]
        )

    def _process_entry_format(self, row):
        return f'''        {{
            "access": "{access_from_code(get_file_info(row[1])[2])}",
            "pid": {row[0]}
        }}'''

    def _file_entry_format(self, row: libetrace.nfsdbEntryOpenfile):
        return '''        {{
            "filename": "{filename}",
            "original_path": "{original_path}",
            "ppid": {ppid},
            "type": "{type}",
            "access": "{access}",
            "exists": {exists},
            "link": {link}
        }}'''.format(
            filename=self.origin_module.get_path(row.path) if self.args.relative else row.path,
            original_path=row.original_path,
            ppid=row.parent.eid.pid,
            type="linked" if row.opaque and row.opaque.is_linking() and row.opaque.linked_path == row.path else "compiled" if row.opaque and row.opaque.compilation_info and row.path in row.opaque.compilation_info.file_paths else "plain",
            access=access_from_code(get_file_info(row.mode)[2]),
            exists=1 if row.exists() else 0,
            link=1 if row.is_symlink() else 0
        )

    def _command_generate_entry_format(self, row:libetrace.nfsdbEntry):
        filename = row.compilation_info.files[0].path if row.compilation_info and len(row.compilation_info.files) > 0 else row.linked_file.path if row.linked_file else ""
        return f'''    {{
        "directory": "{row.cwd}",
        "command": {fix_cmd(row.argv)},
        "file": "{filename}"
    }}'''

    def _command_generate_openrefs_entry_format(self, row:libetrace.nfsdbEntry):
        filename = row.compilation_info.files[0].path if row.compilation_info and len(row.compilation_info.files) > 0 else row.linked_file.path if row.linked_file else ""
        return '''    {{
        "directory": "{dir}",
        "command": {cmd},
        "file": "{filename}",
        "openfiles": [{refs}]
    }}'''.format(
            dir=row.cwd,
            cmd=fix_cmd(row.argv),
            filename=filename,
            refs=("" if not row.opens else '\n            '+',\n            '.join(['"'+o.path+'"' for o in row.opens]) + "\n        ")
        )

    def compilation_db_data_renderer(self):
        return "[\n" + ',\n'.join([self.compilation_db_formatter(row) for row in self.data]) + "\n]"

    def compilation_db_formatter(self, row: Dict[str, str]):
        return f'''    {{\n        "directory": "{row["dir"]}",\n        "command": {row["cmd"]},\n        "file": "{row["filename"]}"\n    }}'''

    def cdm_data_renderer(self):
        return "{\n" + ',\n'.join([self.cdm_formatter(row) for row in self.data]) + "\n}"

    def cdm_formatter(self, row):
        if self.args.sorted:
            return '''    "{filename}": [{deps}]'''.format(
                filename=row[0],
                deps=('\n        '+',\n        '.join(['"' + o + '"' for o in sorted(row[1], reverse=self.args.reverse)]) + "\n    ")
                )
        else:
            return '''    "{filename}": [{deps}]'''.format(
                filename=row[0],
                deps=('\n        '+',\n        '.join(['"' + o + '"' for o in row[1]]) + "\n    ")
                )

    def rdm_data_renderer(self):
        return "{\n" + ',\n'.join([self.rdm_formatter(row) for row in self.data]) + "\n}"

    def rdm_formatter(self, row):
        if self.args.sorted:
            return '''    "{filename}": [{deps}]'''.format(
                filename=row[0],
                deps=('\n        '+',\n        '.join([f'"{o}"' for o in sorted(row[1], reverse=self.args.reverse)]) + "\n    ")
                )
        else:
            return '''    "{filename}": [{deps}]'''.format(
                filename=row[0],
                deps=('\n        '+',\n        '.join([f'"{o}"' for o in row[1]]) + "\n    ")
                )

    def dep_graph_data_renderer(self):
        return '{\n    "dep_graph": {\n' + ',\n'.join([self.dep_graph_formatter(row) for row in self.data]) + '    }\n}'

    def dep_graph_proc_formatter(self, row):
        return '''                [
                    {pid},
                    [
{opens}
                    ],
                    [
{cmd}
                    ]
                ]'''.format(
                    pid=row[0],
                    opens=",\n".join([f'                        "{wr_file}"' for wr_file in row[1]]),
                    cmd=",\n".join([f'                        {fix_cmd(cmd)}' for cmd in row[2]])
                )

    def dep_graph_formatter(self, row):
        return '''        "'''+row[0]+'''": [
            [
'''+",\n".join([self.dep_graph_proc_formatter(ex) for ex in row[1][0]])+'''
            ],
            [
'''+",\n".join(['                        "{}"'.format(writes) for writes in row[1][1]])+'''
            ]
        ]'''

    def source_root_data_renderer(self):
        return json.dumps({"source_root": self.data}, indent=4)

    def root_pid_data_renderer(self):
        return json.dumps({"root_pid": self.data}, indent=4)

    def dbversion_data_renderer(self):
        return json.dumps({"version": self.data}, indent=4)

    def stat_data_renderer(self):
        return json.dumps(self.data, indent=4)

    def config_data_renderer(self):
        if isinstance(self.data, libcas.CASConfig):
            return json.dumps(self.data.config_info, indent=4)
        else:
            return json.dumps({})

    def config_part_data_renderer(self):
        return json.dumps(self.data, indent=4)
