import multiprocessing
import clang
import pickle as cPickle
import os
import subprocess
import libetrace
import sys
import traceback

integrated_clang_compilers = []
clangxx_input_execs = []

def have_integrated_cc1(exe):
    return all([
        "-fno-integrated-cc1" not in exe[2],
        os.path.normpath(os.path.join(exe[1], exe[0])) in integrated_clang_compilers
    ])

def replace_cc1_executor(x):
        if not have_integrated_cc1((x["b"], x["w"], x["v"])):
            return {
                "b": x["b"], 
                "w": x["w"], 
                "v": x["v"],
                "p": x["p"], 
                "x": x["x"],
                "t": x["t"],
                "i": 0
            }

        try:
            pn = subprocess.Popen(
                [x["b"]] + x["v"][1:] + ["-###"], 
                cwd = x["w"], 
                shell=False, 
                stdin=subprocess.PIPE, 
                stdout=subprocess.PIPE, 
                stderr=subprocess.STDOUT
                )
            out, err = pn.communicate("")
        except Exception:
            print("Exception while running clang -### commands:")
            traceback.print_exc()

        if pn and pn.returncode == 0:
            lns = out.decode("utf-8").split("\n")
            idx = [k for k,u in enumerate(lns) if "(in-process)" in u]
            if idx and len(lns) >= idx[0] + 2:
                ncmd = lns[idx[0] + 1]
                try:
                    return { 
                        "b": x["b"], 
                        "w": x["w"], 
                        "v": [
                                u[1:-1].encode('latin1').decode('unicode-escape').encode('latin1').decode('utf-8')
                                for u in libetrace.parse_compiler_triple_hash(ncmd)
                            ], 
                        "p": x["p"], 
                        "x": x["x"],
                        "t": x["t"],
                        "i": 1
                        }
                except Exception as e:
                    print ("Exception while replacing integrated clang invocation:")
                    print ("Original command:")
                    print ("  %s [%s] %s"%(x["b"],x["w"]," ".join(x["v"])))
                    traceback.print_exc()
        return {
            "b": x["b"], 
            "w": x["w"], 
            "v": x["v"],
            "p": x["p"], 
            "x": x["x"],
            "t": x["t"],
            "i": 0
        }


def get_clang_comps(clangxx_input_execs, clang_tailopts,
            clang_compilers, clangpp_compilers, integrated_clang_compilers, start_offset, debug, verbose, debug_compilations, allow_pp_in_compilations, chunk_number  ):

    clang_c = clang.clang(verbose,debug,clang_compilers,clangpp_compilers,integrated_clang_compilers,debug_compilations)

    result = {}
    for i in range(len(clangxx_input_execs)):
        new_exec = (clangxx_input_execs[i][0], clangxx_input_execs[i][1], clangxx_input_execs[i][2], clangxx_input_execs[i][3], clangxx_input_execs[i][5])
        x = clang_c.get_compilations([new_exec], 1, tailopts=clang_tailopts, allowpp=allow_pp_in_compilations)
        if len(x) == 0:
            continue
        result[start_offset + i] = x[0]
    sys.stdout.write('\r-- get_clang_comps: done chunk %d' % (chunk_number))
    return result


def main():
    global integrated_clang_compilers, clangxx_input_execs

    if len(sys.argv) < 2:
        sys.stderr.write("This tool is intented to be used only internally by wrapper script")
        exit(1)

    if 'clang' in sys.argv[1]:
        inFile = sys.argv[2]
        outFile = sys.argv[3]

        with open(inFile, 'rb') as f:
            args = cPickle.load(f)
        job_list = get_clang_comps(**args)

        with open(outFile, 'wb') as f:
            cPickle.dump(job_list, f)
    elif 'cc1_replace' in sys.argv:
        with open('cc1_replace.pkl','rb') as f:
            args = cPickle.load(f)

        integrated_clang_compilers = args["integrated_clang_compilers"]
        clangxx_input_execs = args["clangxx_input_execs"]

        print ("Replacing cc1 calls for integrated cc1 invocation ... (%d compilations; %d jobs)"%(len(clangxx_input_execs),multiprocessing.cpu_count()))
        p = multiprocessing.Pool(multiprocessing.cpu_count())
        job_list = p.map(replace_cc1_executor,  clangxx_input_execs)
        print('-- replace_cc1_executor: Done. Sharing results with parent process')

        with open('cc1_replace_result2.pkl', 'wb') as f:
            cPickle.dump(job_list, f)

if __name__ == '__main__':
    main()
