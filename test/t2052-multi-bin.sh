#! /bin/sh -e

# Multiple output bins don't make sense, so make sure those fail.

. ./tup.sh
cat > Tupfile << HERE
: foreach foo.c bar.c |> gcc -c %f -o %o |> %B.o {objs} {blah}
HERE
tup touch foo.c bar.c Tupfile
parse_fail_msg "bin must be at the end of the line"

eotup
