#! /bin/sh -e

# See if we have a Tupfile in '.' that uses files from 'foo', and a Tupfile in
# 'foo' that uses files from '.' that we get yelled at. This is what we in the
# biz call a "circular dependency".

. ../tup.sh
cat > Tupfile << HERE
: foo/*.o |> gcc %f -o %o |> prog
HERE

mkdir foo
cat > foo/Tupfile << HERE
: foreach ../*.c |> gcc -c %f -o %o |> %B.o
HERE

cat > foo/ok.c << HERE
int main(void) {return 0;}
HERE

tup touch Tupfile foo/Tupfile foo/ok.c
update_fail
