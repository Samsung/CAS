#!/bin/sh
TESTS_DIR=$(dirname "$0")
for f in $TESTS_DIR/*.c; do
	gcc $f -o "$TESTS_DIR"/$(basename "$f" ".c")
done
