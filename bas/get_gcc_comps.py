import multiprocessing
import gcc
import pickle as cPickle
import os
import subprocess
import libetrace
import sys
import traceback

gxx_input_execs = []

def get_gcc_comps(gxx_input_execs,
            gcc_compilers, gpp_compilers, start_offset, debug, verbose, debug_compilations, input_compiler_parser, chunk_number  ):

    gcc_c = gcc.gcc(verbose,debug,gcc_compilers,gpp_compilers,debug_compilations)

    plugin_path = os.path.join(os.path.dirname(sys.argv[0]),"../libgcc_input_name.so")
    plugin_path = "-fplugin=" + os.path.realpath(plugin_path)

    result = {}
    for i in range(len(gxx_input_execs)):
        new_exec = (gxx_input_execs[i][0], gxx_input_execs[i][1], gxx_input_execs[i][2], gxx_input_execs[i][3])
        if input_compiler_parser is None:
            x = gcc_c.get_compilations([new_exec],1,plugin_path)
        else:
            x = gcc_c.get_compilations([new_exec],1,plugin_path,input_compiler_parser)
        if len(x) == 0:
            continue
        result[start_offset + i] = x[0]
    sys.stdout.write('\r-- get_gcc_comps: done chunk %d' % (chunk_number))
    return result


def main():
    global gxx_input_execs

    if len(sys.argv) < 2:
        sys.stderr.write("This tool is intented to be used only internally by wrapper script")
        exit(1)

    if 'gcc' in sys.argv[1]:
        inFile = sys.argv[2]
        outFile = sys.argv[3]

        with open(inFile, 'rb') as f:
            args = cPickle.load(f)
        job_list = get_gcc_comps(**args)

        with open(outFile, 'wb') as f:
            cPickle.dump(job_list, f)

if __name__ == '__main__':
    main()
