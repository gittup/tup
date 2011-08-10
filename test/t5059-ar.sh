#! /bin/sh -e

# Make sure just running a regular ar command will include the proper objects.
# I used to make my ar rules 'rm -f %o; ar ...', but now tup should unlink the
# output file automatically before running the command.
. ./tup.sh

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> ar crs %o %f |> lib.a
HERE
echo "void foo(void) {}" > foo.c
echo "void bar(void) {}" > bar.c
tup touch foo.c bar.c Tupfile
update
sym_check lib.a foo bar

rm foo.c
tup rm foo.c
update
sym_check lib.a ^foo bar

# This is a quick check for commit 794c3ae9ac7bcffe6b6418e4daf03f5c844e1f3a
# to make sure OSX doesn't create .fuse_hidden files from 'ar'.
if ls -la | grep .fuse_hidden > /dev/null; then
	echo "Error: .fuse_hidden file found!" 1>&2
	exit 1
fi

eotup
