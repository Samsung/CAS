#!/usr/bin/env python3

import argparse
import os
import re
import string
import sys
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))

def find_next_dir(root_dir):
    PAT = re.compile(r"([0-9]+)-.+")
    dirs = [d for d in os.listdir(root_dir) if PAT.match(d)]
    dirs.sort(reverse=True)
    target_dir = None
    try:
        m = PAT.match(dirs[0])
        last_number = int(m.group(1)) + 1
        target_dir = f"{last_number:03d}-{args.dir}"
    except IndexError:
        pass
    if not target_dir:
        target_dir = f"001-{args.dir}"
    return os.path.join(root_dir, target_dir)

parser = argparse.ArgumentParser()
parser.add_argument("dir")
args = parser.parse_args()

allowed_chars = string.ascii_lowercase + "-" + string.digits
allowed_chars_set = set(allowed_chars)
if not all(letter in allowed_chars_set for letter in args.dir):
    print(f"[!] Dir {args.dir} contain disallowed characters", file=sys.stderr)
    print(f"    Allowed chars: a-z 0-9 -")
    sys.exit(1)

input_dir = os.path.join(HERE, "template")
target_dir = find_next_dir(HERE)
print("Creating new test database")
print(f"  Source: {input_dir}")
print(f"  Target: {target_dir}")
shutil.copytree(input_dir, target_dir)
