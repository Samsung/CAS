import sys
import os
import io
import zipfile
from typing import Generator, Iterator, Dict, List
import libcas
from libftdb import FtdbError
import libetrace
from libft_db import FTDatabase
from client.misc import printdbg, printerr, get_config_path
from client.mod_base import ModulePipeline
from client.argparser import get_args, merge_args, get_api_keywords


def process_commandline(cas_db: libcas.CASDatabase, commandline: "str | List[str] | None" = None, ft_db:FTDatabase = None) -> "str | Dict | None":
    """
    Main function used to process commandline execution.
    By default it feeds arguments from sys.argv but can be overwritten by optional commandline value

    :param cas_db: database object used for queries
    :type cas_db: libcas.CASDatabase
    :param commandline: optional commandline argument for using programmatically, defaults to None
    :type commandline: str, optional
    :return: rendered string output, object (object renderer) or None
    :rtype: str | object | None
    """
    common_args, pipeline_args, common_parser = get_args(commandline)
    if not cas_db.config:
        config_file = get_config_path(os.path.join(common_args.dbdir, common_args.config))
        printdbg(f"DEBUG: Loading {config_file} config file...", common_args)
        config = libcas.CASConfig(config_file)
        cas_db.set_config(config)

    ftdb_cmds = ["ftdb-version",
                 "ftdb-module",
                 "ftdb-dir",
                 "ftdb-release",
                 "sources",
                 "srcs",
                 "modules",
                 "mds",
                 "functions",
                 "funcs",
                 "globals",
                 "globs",
                 "funcdecls",
                 "types"]

    if len(pipeline_args) > 0:

        if ft_db and not ft_db.db_loaded:
            try:
                ftdb_required = any([x.name in ftdb_cmds for x in pipeline_args])
                if ftdb_required:
                    ft_db.load_db(os.path.join(common_args.ftdb_dir, common_args.ftdb), debug=common_args.debug, quiet=True)
            except FtdbError:
                printerr("ERROR: Failed to load ftdb database. Check if {} is proper database file path.".format(os.path.join(common_args.ftdb_dir, common_args.ftdb)))
                sys.exit(2)

        if not cas_db.db_loaded:
            db_required = not all([x.name in ["parse", "postprocess", "pp", "cache", "config", "cfg", "compiler_pattern", "linker_pattern"]+ftdb_cmds for x in pipeline_args])
            try:
                if db_required:
                    cas_db.load_db(os.path.join(common_args.dbdir, common_args.database), debug=common_args.debug)
            except libetrace.error:
                printerr("ERROR: Failed to load database. Check if {} is proper database file path.".format(os.path.join(common_args.dbdir, common_args.database)))
                sys.exit(2)

            if not os.path.exists(os.path.join(common_args.dbdir, common_args.deps_database)):
                printdbg("WARNING: Cache database '{}' not found - some functionality will be unavailable.".format(os.path.join(common_args.dbdir, common_args.deps_database)), common_args)
            else:
                try:
                    if db_required and cas_db is not None:
                        cas_db.load_deps_db(os.path.join(common_args.dbdir, common_args.deps_database), debug=common_args.debug)
                except libetrace.error:
                    print("ERROR: Failed to load deps database. Check if {} is proper database file path.".format(os.path.join(common_args.dbdir, common_args.deps_database)))
                    sys.exit(2)

        module_pipeline = ModulePipeline([
            pipe_element.module(merge_args(common_args, pipe_element), cas_db, cas_db.config, ft_db)
            for pipe_element in pipeline_args
            ])

        if common_args.ide:
            from client.ide_generator.project_generator import project_generator_map, ProjectGenerator
            project_generator: ProjectGenerator = project_generator_map[common_args.ide](args=common_args, cas_db=cas_db)
            latest_module = module_pipeline.modules[-1]

            if not latest_module.args.show_commands and not latest_module.args.details:
                latest_module.args.details = True
            project_generator.generate(data=module_pipeline.render())
            return None

        elif common_args.output_file:
            with open(os.path.abspath(os.path.expanduser(common_args.output_file)), "w", encoding=sys.getdefaultencoding()) as output_file:
                if output_file.writable():
                    ret = module_pipeline.render()
                    if isinstance(ret, Iterator) or isinstance(ret, Generator):
                        output_file.write(os.linesep.join(ret))
                        print("Output written to {}".format(os.path.abspath(os.path.expanduser(common_args.output_file))))
                    elif isinstance(ret, str):
                        output_file.write(ret)
                        print("Output written to {}".format(os.path.abspath(os.path.expanduser(common_args.output_file))))
                    else:
                        pass
            return None
        elif common_args.generate_zip:
            zip_buffer = io.BytesIO()
            out_file_name = "results.json" if common_args.json else "results.txt"

            with zipfile.ZipFile(zip_buffer, "a", zipfile.ZIP_DEFLATED, False) as zip_file:
                ret = module_pipeline.render()
                if isinstance(ret, Iterator) or isinstance(ret, Generator):
                    zip_file.writestr(out_file_name, os.linesep.join(ret))
                else:
                    zip_file.writestr(out_file_name, ret)

            with open(os.path.abspath(os.path.expanduser(common_args.generate_zip)), 'wb') as out_file:
                out_file.write(zip_buffer.getvalue())
                print("Output zipped to {}".format(os.path.abspath(os.path.expanduser(common_args.generate_zip))))
            return None

        elif common_args.generate_zip_files:
            
            ret = module_pipeline.render()
            print("Writing files to archive ...")
            with zipfile.ZipFile(common_args.generate_zip_files, "w", zipfile.ZIP_DEFLATED) as zip_file:
                for filename in ret:
                    if os.path.exists(filename):
                        relative_path = filename[len(cas_db.source_root):]
                        zip_file.write(filename, arcname=relative_path)
                    else:
                        print(f"WARNING: File does not exists: {filename}")
            print(f"All files written to {common_args.generate_zip_files}")
            return None

        elif common_args.ftdb_create:
            from client.ftdb_generator.ftdb_generator import FtdbGenerator
            latest_module = module_pipeline.modules[-1]
            ftdb_generator = FtdbGenerator(args=common_args, cas_db=cas_db, latest_module=latest_module)
            if not latest_module.args.show_commands and not latest_module.args.details:
                latest_module.args.details = True
            ftdb_generator.create_ftdb(module_pipeline.render())
        else:
            return module_pipeline.render()
    else:
        common_parser.add_argument("module", choices=get_api_keywords(), help="Module or pipeline of modules to process")
        common_parser.print_help()
