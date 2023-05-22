import sys
import os
import io
import zipfile
from typing import Generator, Iterator
import libcas
from client.misc import printdbg, printerr, get_config_path
from client.mod_base import ModulePipeline
from client.argparser import get_args, merge_args, get_api_keywords


def process_commandline(cas_db: libcas.CASDatabase, commandline: "str | List[str] | None" = None):
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
    config_file = get_config_path(os.path.join(common_args.dbdir, common_args.config))
    config = libcas.CASConfig(config_file)

    cas_db.set_config(config)

    db_required = not all([x.name in ["parse", "postprocess", "pp", "cache", "config", "cfg", "compiler_pattern", "linker_pattern"] for x in pipeline_args])

    if len(pipeline_args) > 0:
        if not cas_db.db_loaded:
            try:
                if db_required:
                    cas_db.load_db(os.path.join(common_args.dbdir, common_args.database), debug=common_args.debug, quiet=True, mp_safe=common_args.mp_safe)
            except SystemError:
                printerr("ERROR: Failed to load database. Check if {} is proper database file path.".format(os.path.join(common_args.dbdir, common_args.database)))
                sys.exit(2)

            if not os.path.exists(os.path.join(common_args.dbdir, common_args.deps_database)):
                printdbg("WARNING: Cache database '{}' not found - some functionality will be unavailable.".format(os.path.join(common_args.dbdir, common_args.deps_database)), common_args)
            else:
                try:
                    if db_required and cas_db is not None:
                        cas_db.load_deps_db(os.path.join(common_args.dbdir, common_args.deps_database), debug=common_args.debug, quiet=True, mp_safe=common_args.mp_safe)
                except SystemError:
                    print("ERROR: Failed to load deps database. Check if {} is proper database file path.".format(os.path.join(common_args.dbdir, common_args.deps_database)))
                    sys.exit(2)

        module_pipeline = ModulePipeline([
            pipe_element.module(merge_args(common_args, pipe_element), cas_db, config)
            for pipe_element in pipeline_args
            ])

        if common_args.output_file:
            with open(os.path.abspath(os.path.expanduser(common_args.output_file)), "w", encoding="") as output_file:
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
        else:
            return module_pipeline.render()
    else:
        common_parser.add_argument("module", choices=get_api_keywords(), help="Module or pipeline of modules to process")
        common_parser.print_help()
