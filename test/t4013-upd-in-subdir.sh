#! /bin/sh -e

# Apparently I have a bug where I run the updater in a subdirectory and it
# gets confused and thinks it's writing to the wrong file.

. ../tup.sh

tmkdir a
tmkdir a/b
cp ../testTupfile.tup a/b/Tupfile

echo "int main(void) {}" > a/b/foo.c
tup touch a/b/foo.c a/b/Tupfile
update
sym_check a/b/prog main

tup touch a/b/foo.c
cd a
update
