#! /bin/sh -e

# Not sure how I didn't find this one before - so we have two files that
# include a header. We touch the header, and the first file fails to compile.
# Then we fix the compilation error by changing the C file (not the header) and
# update - the second file won't ever be rebuilt. Obviously, that is incorrect.
#
# Note: I use 'afoo' here because it used to be foo, then I changed how
# foreach works (it now processes files in order, instead of in reverse), and
# it needs to go first.
. ../tup.sh
tup config num_jobs 1
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE

echo '#include "foo.h"' > afoo.c
echo '#include "foo.h"' > bar.c
touch foo.h
tup touch afoo.c bar.c foo.h
update
check_exist afoo.o bar.o
rm -f bar.o
echo 'bork' >> afoo.c
tup touch foo.h
update_fail
check_not_exist bar.o

echo '#include "foo.h"' > afoo.c
tup touch afoo.c
update
check_exist afoo.o bar.o
