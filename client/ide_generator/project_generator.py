from abc import abstractmethod
import sys
import os
import io
import json
import zipfile
from typing import Generator, Iterator
import libetrace
from libcas import CASDatabase

def add_params(parser):
    ide_group = parser.add_argument_group("IDE project generation arguments")

    ide_group.add_argument('--ide','-g', type=str, default=None, choices=['eclipse','vscode'], help='Generate IDE project based on results.')
    ide_group.add_argument('--metaname', type=str, default=None, help='Project name')
    ide_group.add_argument('--remap-source-root', type=str, default=None, help='Change original source root')
    ide_group.add_argument('--include-source', action='store_true', default=False,help='Add source files to project file')
    ide_group.add_argument('--no-asm', action='store_true', default=False, help='Disable asm files')
    ide_group.add_argument('--keep-going', action='store_true', default=False, help='Keep going even if error appear')


class ProjectGenerator:
    def __init__(self, args, cas_db: CASDatabase) -> None:
        self.args = args
        self.db = cas_db
        self.source_root = self.db.source_root
        if not self.args.output_file:
            self.args.output_file = "project.zip"
        if not self.args.metaname:
            self.args.metaname = "cas_project"
        self.workspace_file = "unknown"
        self.zip_buffer = io.BytesIO()
        self.zip_file = zipfile.ZipFile(self.zip_buffer, "a", zipfile.ZIP_BZIP2, False)

    def store_zip_stream(self):
        self.zip_file.close()
        with open(os.path.abspath(os.path.expanduser(self.args.output_file)), 'wb') as out_file:
            out_file.write(self.zip_buffer.getvalue())
            print("IDE Project zipped to {}".format(os.path.abspath(os.path.expanduser(self.args.output_file))))

    def write_file(self, arcname, contents):
        self.zip_file.writestr(data=contents.encode(sys.getdefaultencoding()), zinfo_or_arcname=arcname)

    def add_file(self, arcname, file_path ):
        self.zip_file.write(filename=file_path, arcname=arcname)

    @abstractmethod
    def add_files(self):
        pass
    @abstractmethod
    def add_workspace(self):
        pass

    def generate(self, data):

        print ("Generating IDE project ...")

        if isinstance(data,str):
            self.data = data.split(os.linesep)
        elif isinstance(data, Iterator) or isinstance(data, Generator):
            self.data = list(data)
        else:
            self.data = data

        self.add_files()
        self.add_workspace()

        self.store_zip_stream()

class EclipseProjectGenerator(ProjectGenerator):
    def __init__(self, args, cas_db: CASDatabase) -> None:
        super().__init__(args, cas_db)
        self.workspace_file = f"{args.metaname}.project"

    def add_files(self):
        pass

    def add_workspace(self):
        pass

class VSCodeProjectGenerator(ProjectGenerator):
    def __init__(self, args, cas_db: CASDatabase) -> None:
        super().__init__(args, cas_db)
        self.workspace_file = f"{args.metaname}.code-workspace"
        self.compile_commands = []

    def add_files(self):
        for d in self.data:
            if d.opaque:
                self.compile_commands.append( {"file":d.path,"argument":d.opaque.argv,"cwd":d.opaque.cwd })
                

            print ( d.path.replace(self.source_root, self.args.remap_source_root))
            try:
                self.add_file(arcname=d.path.replace(self.source_root, f"{self.args.metaname}/src"),file_path=d.path.replace(self.source_root, self.args.remap_source_root))
            except:
                pass

    def add_workspace(self):
        w = {
    "settings": {
        "C_Cpp.default.compileCommands" : f"{self.args.metaname}/compile_commands.json"
    },

    "folders": [
        {
        "path": "./src/"
        }
    ]
        }
        self.write_file(f"{self.args.metaname}/{self.args.metaname}.code-workspace", json.dumps(w,indent=4))
        self.write_file(f"{self.args.metaname}/compile_commands.json", json.dumps(self.compile_commands,indent=4))

project_generator_map = {
    "vscode" : VSCodeProjectGenerator,
    "eclipse" : EclipseProjectGenerator
}
