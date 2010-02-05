#! /bin/sh -e

# Now make sure a ghost Tuprules.tup already exists before creating a new
# Tupfile to try to include it.

. ./tup.sh
tmkdir fs
tmkdir fs/sub
cat > fs/Tupfile << HERE
include_rules
: foreach *.c |> gcc \$(CFLAGS) -c %f -o %o |> %B.o
: *.o |> gcc \$(LDFLAGS) %f -o %o |> prog
HERE

tup touch fs/Tupfile
tup touch fs/ok.c
tup touch fs/sub/helper.c
tup parse

tup_dep_exist fs ok.c fs 'gcc  -c ok.c -o ok.o'

tup_dep_exist . Tuprules.tup . fs

cp fs/Tupfile fs/sub/Tupfile
tup touch fs/sub/Tupfile
tup parse

tup_dep_exist fs/sub helper.c fs/sub 'gcc  -c helper.c -o helper.o'

eotup
