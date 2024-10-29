
from typing import Any, Tuple, Callable
import libetrace
from client.mod_base import Module, PipedModule, FilterableModule
from client.misc import printdbg
from client.exceptions import PipelineException
from client.output_renderers.output import DataTypes


class DepsFor(Module, PipedModule, FilterableModule):
    """Dependencies - returns list of all files that have been read in process of creating given file."""

    # example:
    # p# - process
    # f# - file

    # > deps_for file.ko
    #     file.ko - written by p1
    #     p1 reads f1, f2, f3
    #         f1 written by p2
    #             p2 reads f4, f5
    #                 f4 written by p5
    #                     p5 reads f6, f7
    #                         f6 not written by any process
    #                         f7 not written by any process
    #                 f5 not written by any process
    #         f2 written by p3
    #             p3 reads f8
    #                 f8 not written by any process
    #         f3 written by p4
    #             p4 reads f9, f10
    #                 f9 written by p6
    #                     p6 no files read
    #                 f10 written by p7
    #                     p7 reads f11
    #                         f11 not written by any process
    #
    # example return f1 - f11

    required_args = ["path:1+"]

    @staticmethod
    def get_argparser():
        return Module.add_args([
            "filter", "command-filter", "select", "append",
            "details", "commands",
            "path",
            "dep-graph",
            "wrap-deps",
            "with-pipes",
            "timeout",
            "direct-global",
            "rdm",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            "revdeps",
            "recursive",
            "dry-run",
            "debug-fd",
            "match",
            "cached", "cdb", "deep"
            ], DepsFor)

    def select_subject(self, ent) -> str:
        return ent.path

    def exclude_subject(self, ent) -> str:
        return ent.path

    def subject(self, ent):
        if self.args.show_commands:
            return " ".join(ent.argv)
        elif self.args.details:
            return ent.path
        else:
            return ent

    def set_piped_arg(self, data, data_type: type):
        if data_type == str:
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = data
        if data_type == libetrace.nfsdbEntryOpenfile:
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = list({o.path for o in data})
        if data_type == libetrace.nfsdbEntry:
            raise PipelineException("Wrong pipe input data {}.".format(data_type))

    def get_data(self) -> Tuple[Any, DataTypes, "Callable | None", "type|None"]:
        paths = self.get_ext_paths(self.args.path)
        self.args.path = None

        if self.args.dep_graph:
            data = self.get_dependency_graph(paths)
            return data, DataTypes.dep_graph_data, lambda x: x[0], str
        elif self.args.show_commands:
            data = list({
                self.get_exec_of_open(d)
                for d in self.get_multi_deps(paths)
                if self.filter_exec(self.get_exec_of_open(d)) and self.should_display_open(d) and self.filter_open(d)
            })
            if self.args.cdb:
                data = list(self.cdb_fix_multiple(data))
                return data, DataTypes.compilation_db_data, lambda x: x['filename'], str
            return data, DataTypes.commands_data, lambda x: x.eid.pid, libetrace.nfsdbEntry
        elif self.args.details:
            if self.args.deep:
                data = list({
                    f
                    for d in self.get_multi_deps(paths)
                    for f in self.get_deep_comps(d)
                    if self.filter_open(f)
                })
            else:
                data = list({
                    d
                    for d in self.get_multi_deps(paths)
                    if self.filter_open(d)
                })
            return data, DataTypes.file_data, lambda x: x.path, libetrace.nfsdbEntryOpenfile
        else:
            if self.args.deep:
                data = list({
                    f.path
                    for d in self.get_multi_deps(paths)
                    for f in self.get_deep_comps(d)
                    if self.filter_open(f)
                })
            else:
                data = list({
                    d.path
                    for d in self.get_multi_deps(paths)
                    if self.filter_open(d)
                })

            if self.args.rdm:
                data = self.get_rdm(data)
                return data, DataTypes.rdm_data, lambda x: x[0], None
            if self.args.cdm:
                data = self.get_cdm(data)
                return data, DataTypes.cdm_data, lambda x: x[0], None
            if self.args.revdeps:
                data = self.get_revdeps(data)
                return data, DataTypes.file_data, None, str
            return data, DataTypes.file_data, None, str


class RevDepsFor(Module, PipedModule, FilterableModule):
    """Reverse dependencies - returns all files that depends on given file."""

    # example:
    # p# - process
    # f# - file

    # > revdeps_for file.ko
    #     file.ko - read by p1, p2, p3
    #         p1 writes f1, f2
    #             f1 read by p4
    #                 p4 writes f5
    #                     f5 not read by any process
    #             f2 read by p5
    #                 p5 writes f6
    #                     f6 not read by any process
    #         p2 writes f3
    #             f3 not read by any process
    #         p3 writes f4
    #             f4 read by p7
    #                 p7 writes f8
    #                     f8 not read by any process
    # example return f1-f8

    required_args = ["path:1+"]

    @staticmethod
    def get_argparser():
        return Module.add_args([
            "filter", "command-filter", "select", "append",
            "details", "commands",
            "path",
            "recursive",
            "match",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            ], RevDepsFor)

    def select_subject(self, ent) -> str:
        return ent.path

    def exclude_subject(self, ent) -> str:
        return ent.path

    def subject(self, ent) -> str:
        if self.args.show_commands:
            return " ".join(ent.argv)
        else:
            return ent.path

    def set_piped_arg(self, data, data_type: type):
        if data_type == str:
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = data
        if data_type == libetrace.nfsdbEntryOpenfile:
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = list({o.path for o in data})
        if data_type == libetrace.nfsdbEntry:
            raise PipelineException("Wrong pipe input data {}.".format(data_type))

    def get_data(self) -> Tuple[Any, DataTypes, "Callable | None", "type|None"]:
        rdeps = [f
            for fp in self.nfsdb.db.rdeps(self.args.path, recursive=self.args.recursive)
            for f in self.nfsdb.db.filemap[fp]
        ]

        if self.args.show_commands:
            data = list({
                self.get_exec_of_open(o)
                for o in rdeps
                if self.filter_exec(self.get_exec_of_open(o))
            })

            return data, DataTypes.commands_data, lambda x: x.eid.pid, libetrace.nfsdbEntry
        elif self.args.details:
            data = list({
                o
                for o in rdeps
                if self.filter_open(o)
            })

            return data, DataTypes.file_data, lambda x: x.path, libetrace.nfsdbEntryOpenfile
        else:
            data = list({
                o.path
                for o in rdeps
                if self.filter_open(o)
            })

            if self.args.cdm:
                data = self.get_cdm(data)
                return data, DataTypes.cdm_data, lambda x: x[0], None

            return data, DataTypes.file_data, None, str
