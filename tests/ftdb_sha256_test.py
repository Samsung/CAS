#!/usr/bin/env python3

import libftdb
import hashlib
import sys

ftdb = libftdb.ftdb()
ftdb.load(sys.argv[1])

m = hashlib.sha256()

for f in ftdb.funcs:
                m.update(f.body.encode('ascii'))
                m.update(f.unpreprocessed_body.encode('ascii'))
                for D in f.derefs:
                                m.update(D.kind.to_bytes(8,byteorder='little'))
                for T in f.types:
                                if "def" in ftdb.types[T]:
                                                m.update(ftdb.types[T].defstring.encode('ascii'))

for g in ftdb.globals:
                if "def" in ftdb.types[g.type]:
                                m.update(ftdb.types[g.type].defstring.encode('ascii'))

print (m.hexdigest())
