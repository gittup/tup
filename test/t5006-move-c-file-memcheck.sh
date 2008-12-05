#! /bin/sh -e
# This is basically t5003, but after creating a new file and deleting an old
# file we run with a memory checker. I tried to get an example that exercises
# a fair bit of the usual functionality in a single trace.

. ../tup.sh
cp ../testTupfile Tupfile

# Verify both files are compiled
echo "int main(void) {return 0;}" > foo.c
echo "void bar1(void) {}" > bar.c
tup touch foo.c bar.c
update
sym_check foo.o main
sym_check bar.o bar1
sym_check prog main bar1

# Rename bar.c to realbar.c.
mv bar.c realbar.c
tup delete bar.c
tup touch realbar.c
MALLOC_TRACE=mout tup upd

# Still seem to be some leaks in sqlite, even though I'm finalizing the
# statements and doing an sqlite3_close(). Maybe I'm missing something.
cat mout | grep -v libsqlite3.so | mtrace `which tup` -
