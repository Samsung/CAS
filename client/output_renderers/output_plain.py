import os
import json
from typing import Tuple

import libetrace
import libftdb
import libft_db
import libcas

from client.output_renderers.output import OutputRenderer
from client.misc import access_from_code, get_file_info, stat_from_code


class Renderer(OutputRenderer):
    """
    Plain output - targeted for using in terminal.
    """

    help = 'Prints output line-by-line (generated from output_renderers dir)'
    default_entries_count = 0
    output_formats = {
        "default-output": {
            "linked_view": {
                "fmt": "{L}",
                "entry-fmt": "{f}",
                "entry-detail-fmt": "{f}{sep}{o}{sep}{p}{sep}{t}{sep}{m}{sep}{e}{sep}{s}"
            },
            "file_view": {
                "fmt": "{L}",
                "entry-fmt": "{f}",
                "entry-detail-fmt": "{f}{sep}{o}{sep}{p}{sep}{t}{sep}{m}{sep}{e}{sep}{s}"
            },
            "process_list_view": {
                "fmt": "{L}",
                "entry-fmt": "{p}{sep}{m}",
                "entry-detail-fmt": "{p}{sep}{m}",
            },
            "binary_view": {
                "fmt": "{L}",
                "entry-fmt": "{b}",
                "entry-detail-fmt": "{p}{sep}{x}{sep}{s}{sep}{e}{sep}{r}{sep}{b}{sep}{w}{sep}{v}{sep}{o}",
            },
            "commands_view": {
                "fmt": "{L}",
                "entry-fmt": "{p}{sep}{x}{sep}{s}{sep}{e}{sep}{r}{sep}{b}{sep}{w}{sep}{v}{sep}{l}{sep}{c}{sep}{o}",
                "entry-detail-fmt": "{p}{sep}{x}{sep}{s}{sep}{e}{sep}{r}{sep}{b}{sep}{w}{sep}{v}{sep}{l}{sep}{c}{sep}{o}",
            },
            "compiled_view": {
                "fmt": "{L}",
                "entry-fmt": "{f}",
                "entry-detail-fmt": "{f}{sep}{o}{sep}{p}{sep}{t}{sep}{m}{sep}{e}{sep}{s}"
            },
            "compilation_info_view": {
                "fmt": "{L}",
                "entry-fmt": "<entry_format_spec>",
                "entry-detail-fmt": "<entry_format_spec>"
            },
            "src_md_view": {
                "fmt": "{L}",
                "entry-fmt": "{f}",
                "entry-detail-fmt": "{id}{sep}{f}",
            },
        }
    }

    @staticmethod
    def append_args(parser):
        o = parser.add_argument_group("Plain output arguments")
        o.add_argument('--fmt', type=str, default=None, help='Override list rendering schema')
        o.add_argument('--entry-fmt',  type=str, default=None, help='Override entry rendering schema')
        o.add_argument('--entry-detail-fmt',  type=str, default=None, help='Override detailed entry rendering schema')
        o.add_argument('--separator', '--sep', type=str, default=",", help="Specify separator")

    def count_renderer(self):
        return str(self.count)

    def formatter(self, fmt, entry_fmt):
        if self.count > 0:
            if isinstance(self.data, libetrace.nfsdbFilteredOpensPathsIter):
                if "{S}" in fmt:
                    yield str(self.count)
                if "{N}" in fmt:
                    yield str(self.num_entries)
                if "{L}" in fmt:
                    for row in self.data:
                        yield self.origin_module.get_path(row)
            elif isinstance(self.data, libetrace.nfsdbFilteredOpensIter) or isinstance(self.data, libetrace.nfsdbOpensIter):
                if "{S}" in fmt:
                    yield str(self.count)
                if "{N}" in fmt:
                    yield str(self.num_entries)
                if "{L}" in fmt:
                    for row in self.data:
                        yield self._file_entry_format(row, entry_fmt)
            elif isinstance(self.data, libetrace.nfsdbFilteredCommandsIter):
                if "{S}" in fmt:
                    yield str(self.count)
                if "{N}" in fmt:
                    yield str(self.num_entries)
                if "{L}" in fmt:
                    for row in self.data:
                        yield self._command_entry_format(row, entry_fmt)
            elif isinstance(self.data[0], str):
                if "{S}" in fmt:
                    yield str(self.count)
                if "{N}" in fmt:
                    yield str(self.num_entries)
                if "{L}" in fmt:
                    for row in self.data:
                        yield self.origin_module.get_path(row) if self.args.relative else row
            elif isinstance(self.data[0], libetrace.nfsdbEntryOpenfile):
                if "{S}" in fmt:
                    yield str(self.count)
                if "{N}" in fmt:
                    yield str(self.num_entries)
                if "{L}" in fmt:
                    for row in self.data:
                        yield self._file_entry_format(row, entry_fmt)
            elif isinstance(self.data[0], libetrace.nfsdbEntry):
                if "{S}" in fmt:
                    yield str(self.count)
                if "{N}" in fmt:
                    yield str(self.num_entries)
                if "{L}" in fmt:
                    for row in self.data:
                        yield self._command_entry_format(row, entry_fmt)
            elif isinstance(self.data[0], tuple):
                if "{S}" in fmt:
                    yield str(self.count)
                if "{N}" in fmt:
                    yield str(self.num_entries)
                if "{L}" in fmt:
                    for row in self.data:
                        yield self._process_entry_format(row, entry_fmt)
            elif isinstance(self.data[0],libft_db.ftdbSourceEntry) or isinstance(self.data[0],libft_db.ftdbModuleEntry):
                if "{S}" in fmt:
                    yield str(self.count)
                if "{N}" in fmt:
                    yield str(self.num_entries)
                if "{L}" in fmt:
                    for row in self.data:
                        yield self._src_md_entry_format(row, entry_fmt)
            else:
                assert False, "UNKNOWN RETURN DATA - This should never happend!"
        else:
            return None

    def _get_configured_formatters(self, view_name):
        if self.args.fmt:
            fmt = self.args.fmt.replace("\\n", os.linesep)
        else:
            fmt = self.output_formats["default-output"][view_name]["fmt"]
        if self.args.details:
            if self.args.entry_detail_fmt:
                entry_fmt = self.args.entry_detail_fmt.replace("\\n", os.linesep)
            else:
                entry_fmt = self.output_formats["default-output"][view_name]["entry-detail-fmt"]
        elif self.args.generate:
            if self.args.openrefs:
                entry_fmt = "{b}{sep}{w}{sep}{v}{sep}{n}"
            else:
                entry_fmt = "{b}{sep}{w}{sep}{v}"
        else:
            if self.args.entry_fmt:
                entry_fmt = self.args.entry_fmt.replace("\\n", os.linesep)
            else:
                entry_fmt = self.output_formats["default-output"][view_name]["entry-fmt"]
        return fmt, entry_fmt

    def _file_entry_format(self, row: libetrace.nfsdbEntryOpenfile, entry_fmt):
        return entry_fmt.format(
            f=self.origin_module.get_path(row.path) if self.args.relative else row.path,
            c="linked" if row.opaque and row.opaque.is_linking() and row.opaque.linked_path == row.path else "compiled" if row.opaque and row.opaque.has_compilations() and row.opaque.compilation_info.file_paths[0] == row.path else "plain",
            o=row.original_path,
            p=row.parent.eid.pid,
            t=stat_from_code(get_file_info(row.mode)[1]),
            m=access_from_code(get_file_info(row.mode)[2]),
            e=1 if row.exists() else 0,
            l=row.is_symlink(),
            s=row.size if row.size else -1,
            sep=self.args.separator
        )

    def _command_entry_format(self, row: libetrace.nfsdbEntry, entry_fmt):
        return entry_fmt.format(
            t="compilation" if row.compilation_info is not None else "linker" if row.linked_file is not None else "command",
            p=row.eid.pid,
            x=row.eid.index,
            r=f"{row.parent_eid.pid}:{row.parent_eid.index}",
            b=row.bpath,
            w=row.cwd,
            s=row.stime,
            e=row.etime,
            v=" ".join(row.argv).replace("\n", "\\n"),
            n=",".join(row.openpaths),
            l="None" if row.linked_file is None else row.linked_file.path,
            c="None" if row.compilation_info is None or len(row.compilation_info.files) == 0 else "{}:{}".format(row.compilation_info.files[0].path, {1: "c", 2: "c++", 3: "other"}[row.compilation_info.type]),
            o="None" if not (row.compilation_info is not None and len(row.compilation_info.objects) > 0) else row.compilation_info.objects[0].path,
            sep=self.args.separator
        )

    def _process_entry_format(self, row, entry_fmt):
        return entry_fmt.format(p=row[0], m=access_from_code(get_file_info(row[1])[2]), sep=self.args.separator)
    
    def _src_md_entry_format(self, row:"libft_db.ftdbSourceEntry | libft_db.ftdbModuleEntry", entry_fmt: str) -> str:
        return entry_fmt.format(
            f=row.path,
            sep=self.args.separator,
            id=row.fid if type(row) == libft_db.ftdbSourceEntry else row.mid
        )

    def linked_data_renderer(self):
        fmt, entry_fmt = self._get_configured_formatters("linked_view")
        return self.formatter(fmt, entry_fmt)

    def commands_data_renderer(self):
        if self.args.generate and self.args.makefile:
            return self.makefile_data_renderer()
        fmt, entry_fmt = self._get_configured_formatters("commands_view")
        return self.formatter(fmt, entry_fmt)

    def file_data_renderer(self):
        fmt, entry_fmt = self._get_configured_formatters("file_view")
        return self.formatter(fmt, entry_fmt)

    def process_data_renderer(self):
        fmt, entry_fmt = self._get_configured_formatters("process_list_view")
        return self.formatter(fmt, entry_fmt)

    def binary_data_renderer(self):
        fmt, entry_fmt = self._get_configured_formatters("binary_view")
        return self.formatter(fmt, entry_fmt)

    def compiled_data_renderer(self):
        fmt, entry_fmt = self._get_configured_formatters("compiled_view")
        return self.formatter(fmt, entry_fmt)

    def compilation_info_data_renderer(self):
        for row in self.data:
            row = row.opaque
            yield "COMPILATION:"
            yield "  PID: {}, {}".format(row.eid.pid, row.eid.index)
            yield "  PARENT_PID: {}, {}".format(row.parent_eid.pid, row.parent_eid.index)
            yield "  BIN: {}".format(row.bpath)
            yield "  CWD: {}".format(row.cwd)
            yield "  CMD: {}".format(" ".join(row.argv).replace("\n", "\\n"))
            yield "  SOURCE_TYPE: {}".format({1: "c", 2: "c++", 3: "other"}[row.compilation_info.type])

            yield "  COMPILED_FILES:"
            for pth in sorted(row.compilation_info.files):
                yield "    {}".format(pth.path)

            yield "  HEADERS:"
            for pth in sorted(row.compilation_info.headers):
                yield "    {}".format(pth)

            yield "  INCLUDE_PATHS:"
            for ipt in sorted(row.compilation_info.ipaths):
                yield "    {}".format(ipt)

            yield "  DEFS:"
            for dfs in sorted(row.compilation_info.defs):
                yield "   {} = '{}'".format(dfs[0], dfs[1])

            yield "  UNDEFS:"
            for udf in sorted(row.compilation_info.undefs):
                yield "    {} = '{}'".format(udf[0], udf[1])

    def source_root_data_renderer(self):
        return self.data

    def root_pid_data_renderer(self):
        return str(self.data)

    def dbversion_data_renderer(self):
        return self.data
    
    def modulename_data_renderer(self):
        return self.data

    def dirname_data_renderer(self):
        return self.data

    def releasename_data_renderer(self):
        return self.data

    def stat_data_renderer(self):
        if isinstance(self.data, dict):
            yield "Execs:          {:>10}".format(self.data['execs'])
            yield " - commands:    {:>10}".format(self.data['execs_commands'])
            yield " - compilations:{:>10}".format(self.data['execs_compilations'])
            yield " - linking:     {:>10}".format(self.data['execs_linking'])
            yield "Binaries:       {:>10}".format(self.data['binaries'])
            yield "Opens:          {:>10}".format(self.data['opens'])
            yield " - linked:      {:>10} (unique paths {})".format(self.data['linked'] , self.data['linked_paths'])
            yield " - compiled:    {:>10} (unique paths {})".format(self.data['compiled'], self.data['compiled_paths'])

    def config_data_renderer(self):
        if isinstance(self.data, libcas.CASConfig):
            ret = "Config file: {}\n\n".format(self.data.config_file)
            ret += json.dumps(self.data.config_info, indent=4)
            return ret
        else:
            return None

    def config_part_data_renderer(self):
        return json.dumps(self.data, indent=4)

    def dep_graph_data_renderer(self):
        for row in self.data:
            yield row[0] + "\n  WRITING_PIDS:"
            for ent in (sorted(row[1][0]) if self.args.sorted else row[1][0]):
                yield "    " + str(ent[0]) + \
                    "\n      OPENS:" + \
                    ( "\n" if len(ent[1]) else "") + "\n".join([ "        " + opn for opn in ent[1]]) + \
                    "\n      CMD:" + \
                    ( "\n" if len(ent[2]) else "")  + "\n".join([ "        " + cmd.replace("\n", "\\n") for cmd in ent[2]])
            yield "  ALL_PROCESS_WRITES:" + \
                ( "\n" if len(row[1][1])else "")  + "\n".join([ "    " + wrt for wrt in (sorted(row[1][1]) if self.args.sorted else row[1][1])])

    def rdm_data_renderer(self):
        if self.args.sorted:
            for row in self.data:
                yield row[0]
                for opn in sorted(row[1], reverse=self.args.reverse):
                    yield '  {}'.format(opn)
        else:
            for row in self.data:
                yield row[0]
                for opn in row[1]:
                    yield '  {}'.format(opn)

    def cdm_data_renderer(self):
        if self.args.sorted:
            for row in self.data:
                yield row[0]
                for opn in sorted(row[1], reverse=self.args.reverse):
                    yield '  {}'.format(opn)
        else:
            for row in self.data:
                yield row[0]
                for opn in row[1]:
                    yield '  {}'.format(opn)

    def compilation_db_data_renderer(self):
        return "Compilation db available in --json output"

    def sources_data_renderer(self):
        fmt, entry_fmt = self._get_configured_formatters("src_md_view")
        return self.formatter(fmt, entry_fmt)

    def function_data_renderer(self):
        if self.args.body:
            for func in self.data:
                yield func.body

        elif self.args.ubody:
            for func in self.data:
                yield func.unpreprocessed_body

        else:
            for func in self.data:
                yield func.name
    
    def global_data_renderer(self):
        if self.args.definition:
            for glob in self.data:
                yield glob.defstring
        else:
            for glob in self.data:
                yield glob.name
    
    def modules_data_renderer(self):
        fmt, entry_fmt = self._get_configured_formatters("src_md_view")
        return self.formatter(fmt, entry_fmt)
    
    def funcdecl_data_renderer(self):
        if self.args.declbody:
            for fd in self.data:
                yield fd.decl
        else:
            for fd in self.data:
                yield fd.name
    
    def types_data_renderer(self):
        for t in self.data:
            try:
                yield t.name
            except libftdb.FtdbError:
                pass
