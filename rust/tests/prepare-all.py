#!/usr/bin/env python3

import os
import re
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
PAT = re.compile(r'[0-9]+-[a-z0-9-]+')
logfile = os.path.join(HERE, 'prepare.log')
with open(logfile, 'w+') as fp:
    dirs = os.listdir(HERE)
    dirs.sort()
    for path in dirs:
        fullpath = os.path.join(HERE, path)
        if os.path.isdir(fullpath) and PAT.match(path) is not None:
            print(f"Building database for test case: {path}")
            fp.write(f"------------- Preparing database for test case{path} ------------\n")
            fp.flush()
            subprocess.run(["make", "-s", "-C", fullpath, 'clean'], stdout=fp, stderr=fp)
            subprocess.run(["make", "-s", "-C", fullpath], stdout=fp, stderr=fp)
            fp.write("\n\n\n")
