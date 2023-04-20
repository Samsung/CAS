import argparse
import multiprocessing
import os
import sys
import time
import libcas

from client.mod_base import Module
from client.output_renderers.output import DataTypes


class ParseDB(Module):
    """
    Module used to process raw tracer log to database image file.

    Consumes:
    - .nfsdb

    Produces:
    - .nfsdb.img.tmp
    """
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION ")
        arg_group = module_parser.add_argument_group("DB Parsing arguments")
        arg_group.add_argument('--force', action='store_true', default=False)
        return module_parser

    def get_data(self) -> tuple:
        total_start_time = time.time()
        json_db_filename = os.path.abspath(os.path.join(self.args.dbdir, self.args.json_database))
        tracer_db_filename = os.path.abspath(os.path.join(self.args.dbdir, self.args.tracer_database))
        intm_db_filename = os.path.abspath(os.path.join(self.args.dbdir, ".nfsdb.img.tmp"))

        if not os.path.exists(tracer_db_filename):
            print("ERROR: {} database not found!".format(tracer_db_filename))
            sys.exit(2)

        if not os.path.exists(json_db_filename) or self.args.force:
            print("Generating {} from {}".format(json_db_filename, tracer_db_filename))
            libcas.CASDatabase.parse_trace_to_json(tracer_db_filename, json_db_filename)
            print("Done parse [%.2fs]" % (time.time()-total_start_time))
        else:
            print ("Json database found ( use --force to rebuild )")

        if not os.path.exists(intm_db_filename) or self.args.force:
            src_root = libcas.CASDatabase.get_src_root_from_tracefile(tracer_db_filename)
            if os.path.exists(json_db_filename) and src_root is not None:
                total_start_time = time.time()
                print("Generating intermediate cache")
                libcas.CASDatabase.create_db_image(json_db_filename, src_root, "", [], intm_db_filename, self.args.debug)
                print("Done parse [%.2fs]" % (time.time()-total_start_time))
        else:
            print ("Intermediate database found ( use --force to rebuild )")

        return None, DataTypes.null_data, None


class Postprocess(Module):
    """
    Module used to process information about:
    - linked files
    - reverse binary mappings
    - command patterns
    - compilations.

    Consumes:
    - .nfsdb.img.tmp

    Produces:
    - .nfsdb.comps.json
    - .nfsdb.rbm.json
    - .nfsdb.pcp.json
    - .nfsdb.link.json
    - .nfsdb.json
    """
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION ")
        arg_group = module_parser.add_argument_group("Postprocess arguments")
        arg_group.add_argument('--deps-threshold', '-DT', type=int, default=90000, help='')
        arg_group.add_argument('--allow-pp-in-compilations', '-ap', action="store_true", default=False, help="Compute compiler matches with '-E' as compilations")
        arg_group.add_argument('--no-auto-detect-icc', '-na', action='store_true', default=False)
        arg_group.add_argument('--debug-compilations', action='store_true', default=False)
        arg_group.add_argument('--max-chunk-size', type=int, default=sys.maxsize, help='')
        arg_group.add_argument("--new-database", action="store_true", default=False, help="Recompute database components during update")

        arg_group.add_argument('--compilations', '-c', action='store_true', default=False)
        arg_group.add_argument('--linking', '-l', action='store_true', default=False)
        arg_group.add_argument('--module-dependencies', '-L', action='store_true', default=False, help="")
        arg_group.add_argument('--rbm', action='store_true', default=False, help="")

        arg_group.add_argument('--rbm-wrapping-binaries', type=str, default=None, help="")
        arg_group.add_argument('--precompute-command-patterns', '--pcp', action='store_true', default=False, help="")
        arg_group.add_argument("--link-deps", action="store_true", default=False, help="Compute dependencies for all linked modules")

        arg_group.add_argument('--no-update', '-U', action='store_true', default=False)
        arg_group.add_argument('--force', action='store_true', default=False)
        return module_parser

    def get_data(self) -> tuple:
        total_start_time = time.time()
        json_db_filename = os.path.abspath(os.path.join(self.args.dbdir, self.args.json_database))
        tracer_db_filename = os.path.abspath(os.path.join(self.args.dbdir, self.args.tracer_database))
        intm_db_filename = os.path.abspath(os.path.join(self.args.dbdir, ".nfsdb.img.tmp"))
        workdir = os.path.dirname(json_db_filename)

        src_root = self.args.set_root if self.args.set_root is not None else libcas.CASDatabase.get_src_root_from_tracefile(tracer_db_filename)

        if not src_root:
            print("ERROR: source_root is empty - check first line of tracer_database or provide it with --set-root")
            sys.exit(2)

        self.config.apply_source_root(src_root)

        if not self.args.jobs:
            self.args.jobs = multiprocessing.cpu_count()
        else:
            self.args.jobs = int(self.args.jobs)

        if not os.path.exists(json_db_filename):
            print("ERROR: {} json database not found!".format(json_db_filename))
            sys.exit(2)

        if self.args.rbm_wrapping_binaries:
            wrapping_bin_list = self.args.rbm_wrapping_binaries.split(":")
        else:
            wrapping_bin_list = self.config.rbm_wrapping_binaries

        db = libcas.CASDatabase()
        db.set_config(self.config)
        db.post_process(json_db_filename=json_db_filename, wrapping_bin_list=wrapping_bin_list, workdir=workdir,
            process_linking=self.args.linking, process_comp=self.args.compilations, process_rbm=self.args.rbm, process_pcp=self.args.precompute_command_patterns,
            new_database=self.args.new_database, no_update=self.args.no_update, no_auto_detect_icc=self.args.no_auto_detect_icc,
            max_chunk_size=self.args.max_chunk_size, allow_pp_in_compilations=self.args.allow_pp_in_compilations, jobs=self.args.jobs,
            debug_compilations=self.args.debug_compilations, debug=self.args.debug, verbose=self.args.verbose)

        print("Done postprocess [%.2fs]" % (time.time()-total_start_time))
        return None, DataTypes.null_data, None


class StoreCache(Module):
    """
    Module used store processed information from postprocess step and calculate dependencies.

    Consumes:
    - .nfsdb.json
    - .nfsdb.comps.json
    - .nfsdb.rbm.json
    - .nfsdb.pcp.json
    - .nfsdb.link.json

    Produces:
    - .nfsdb.img
    """
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description ="TODO DESCRIPTION ")
        arg_group = module_parser.add_argument_group("Cache DB arguments")
        arg_group.add_argument('--create', '-C', action='store_true', default=False)
        arg_group.add_argument('--deps-create', '-DC', action='store_true', default=False)
        arg_group.add_argument('--deps-threshold', '-DT', type=int, default=90000, help='')
        arg_group.add_argument('--set-version', type=str, default="", help='Optional string used to identify database')
        arg_group.add_argument('--exclude-command-patterns', type=str, default=None, help="Provide list of patterns to precompute matching with all commands (delimited by ':')" )
        return module_parser

    def get_data(self) -> tuple:
        total_start_time = time.time()
        json_db_filename = os.path.abspath(os.path.join(self.args.dbdir, self.args.json_database))
        cache_db_filename = os.path.abspath(os.path.join(self.args.dbdir, self.args.database))
        deps_cache_db_filename = os.path.abspath(os.path.join(self.args.dbdir, self.args.deps_database))
        tracer_db_filename = os.path.abspath(os.path.join(self.args.dbdir, self.args.tracer_database))
        workdir = os.path.dirname(json_db_filename)
        depmap_filename = os.path.join(workdir, ".nfsdb.depmap.json")
        ddepmap_filename = os.path.join(workdir, ".nfsdb.ddepmap.json")

        src_root = self.args.set_root if self.args.set_root is not None else libcas.CASDatabase.get_src_root_from_tracefile(tracer_db_filename)

        if not src_root:
            print("ERROR: source_root is empty - check first line of tracer_database or provide it with --set-root")
            sys.exit(2)

        if self.args.create or (not self.args.deps_create and not self.args.create):
            if self.args.exclude_command_patterns is None:
                self.args.exclude_command_patterns = []
            else:
                self.args.exclude_command_patterns = self.args.exclude_command_patterns.split(":")
            start_time = time.time()
            print("creating cache from json database ...")
            libcas.CASDatabase.create_db_image(json_db_filename, src_root, self.args.set_version, self.args.exclude_command_patterns, cache_db_filename, self.args.debug)
            print("cache stored [%.2fs]" % (time.time() - start_time))

        if self.args.deps_create or (not self.args.deps_create and not self.args.create):
            start_time = time.time()
            print("creating deps cache from cached database")
            if not os.path.exists(cache_db_filename):
                print("ERROR: No cache data found - use --create first.")

            db = libcas.CASDatabase()
            db.set_config(self.config)
            db.load_db(cache_db_filename, debug=self.args.debug, quiet=True, mp_safe=True, no_map_memory=False)
            db.create_deps_db_image(deps_cache_db_filename=deps_cache_db_filename, depmap_filename=depmap_filename,
                ddepmap_filename=ddepmap_filename, jobs=self.args.jobs, deps_threshold=self.args.deps_threshold,
                debug=self.args.debug)
            print("deps stored [%.2fs]" % (time.time() - start_time))

        print("Done cache [%.2fs]" % (time.time() - total_start_time))
        return None, DataTypes.null_data, None
