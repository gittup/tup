#! /bin/sh -e

# Multiple output bins don't make sense, so make sure those fail.

. ./tup.sh
cat > Tupfile << HERE
: foreach foo.c bar.c |> gcc -c %f -o %o |> %B.o {objs} {blah}
HERE
tup touch foo.c bar.c Tupfile
parse_fail "Shouldn't be able to specify multiple output bins"

eotup
