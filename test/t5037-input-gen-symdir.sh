#! /bin/sh -e

# Make sure we can't use input from a generated symlink to a directory. This
# would be problematic since we don't know at parse time that an output file
# will necessarily be a symlink to a directory, so we can't figure out which
# files to use as input.

. ../tup.sh

tmkdir arch-x86
cat > Tupfile << HERE
: |> ln -s arch-x86 %o |> arch
HERE
tup touch Tupfile arch-x86/foo.c
update

cat > Tupfile << HERE
: |> ln -s arch-x86 %o |> arch
: arch/*.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch Tupfile
parse_fail "Shouldn't be able to use a generated symlink in input specification."
