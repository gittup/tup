#! /bin/sh -e

# Make sure that if we remove a header so the compilation falls through to the
# next header, the first one becomes a ghost (the dependency should still be
# there).
. ./tup.sh

tmkdir foo
tmkdir bar
echo '#define FOO 3' > foo/me.h
echo '#define FOO 5' > bar/me.h
echo '#include "me.h"' > ok.c

cat > Tupfile << HERE
: ok.c |> gcc -c %f -o %o -Ifoo -Ibar |> ok.o
HERE
tup touch foo/me.h bar/me.h ok.c Tupfile
update

tup_dep_exist foo me.h . 'gcc -c ok.c -o ok.o -Ifoo -Ibar'
tup_dep_no_exist bar me.h . 'gcc -c ok.c -o ok.o -Ifoo -Ibar'

rm foo/me.h
tup rm foo/me.h
update

tup_dep_exist foo me.h . 'gcc -c ok.c -o ok.o -Ifoo -Ibar'
tup_dep_exist bar me.h . 'gcc -c ok.c -o ok.o -Ifoo -Ibar'

eotup
