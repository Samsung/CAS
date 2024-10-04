all: db.img

.nfsdb: main.c
	etrace make main

.nfsdb.img: .nfsdb
	cas parse
	cas pp
	cas cache

compile_commands.json: .nfsdb.img
	${CAS_DIR}/examples/extract-cas-info-for-ftdb .nfsdb.img

db.json: compile_commands.json .nfsdb.img
	${CAS_DIR}/clang-proc/create_json_db \
		-P ${CAS_DIR}/clang-proc/clang-proc \
		-m "$$(basename $$(pwd -P))" \
		-V "$$(git rev-parse HEAD | cut -c-8)" \
		--exit-on-error

db.img: db.json
	${CAS_DIR}/tests/ftdb_cache_test --only-ftdb-create db.json

main: main.c
	$(CC) -Wall -O0 main.c -o main

clean:
	rm -rf .cache *.o *.s .nfsdb* fops.json compile_commands.*
	rm -f main db.img db.json

PHONY: clean clean-temps
