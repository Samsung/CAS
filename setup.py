import os
import pathlib
from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext as build_ext_orig


class CMakeExtension(Extension):
    def __init__(self, name, makefile_target):
        self.makefile_target = makefile_target
        super().__init__(name, sources=[])


class CMakeBuild(build_ext_orig):
    def build_extension(self, ext: CMakeExtension):
        cwd = pathlib.Path().absolute()

        build_temp = pathlib.Path.joinpath(pathlib.Path().absolute(), "build_release")
        build_temp.mkdir(parents=True, exist_ok=True)
        ext_fullpath = pathlib.Path.cwd() / self.get_ext_fullpath(ext.name)
        extdir = ext_fullpath.parent.resolve()
        os.chdir(str(build_temp))
        if not pathlib.Path.exists(pathlib.Path('Makefile')):
            self.spawn(['cmake', f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}", '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++', '..'])
        self.spawn(['make', '-j16', ext.makefile_target])
        os.chdir(str(cwd))


setup(
    name='libcas',
    version='1.0',
    packages=['client','client.output_renderers','bas', ''],
    url='https://github.sec.samsung.net/CO7-SRPOL-Mobile-Security/CAS-OSS',
    author="MobileSecurity",
    author_email='mbsec@samsung.com',
    scripts=['cas'],
    ext_modules=[
        CMakeExtension(name="libetrace", makefile_target="etrace"),
        CMakeExtension(name="libftdb", makefile_target="ftdb")],
    cmdclass={
        'build_ext': CMakeBuild,
    }
)

# run it with 
# rm -r build/ build_release/ dist/ libcas.egg-info/ && python3 -m build -w --no-isolation