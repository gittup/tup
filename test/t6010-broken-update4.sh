#! /bin/sh -e

# Yet another broken update test, probably introduced from the last two. So now
# if we generate a few commands from a Tupfile (they're all modify), and the
# first one fails, so we change the Tupfile to create all new commands, the old
# ones are marked delete *and* modify, and they don't actually get removed.
# They should still be deleted.
. ../tup.sh
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o -Dbork=<Bork> |> %F.o
HERE

echo "void foo(void) {}" > foo.c
echo "void bar(void) {}" > bar.c
tup touch foo.c bar.c Tupfile
update_fail
check_not_exist foo.o bar.o

tup_object_exist . 'gcc -c foo.c -o foo.o -Dbork=<Bork>'
tup_object_exist . 'gcc -c bar.c -o bar.o -Dbork=<Bork>'

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %F.o
HERE
tup touch Tupfile
tup upd
sym_check foo.o foo
sym_check bar.o bar

tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'
tup_object_no_exist . 'gcc -c foo.c -o foo.o -Dbork=<Bork>'
tup_object_no_exist . 'gcc -c bar.c -o bar.o -Dbork=<Bork>'
