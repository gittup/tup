#! /bin/sh -e

# Make sure include_rules makes ghost Tuprules.tup files.

. ./tup.sh
tmkdir fs
tmkdir fs/sub
cat > fs/sub/Tupfile << HERE
include_rules
: foreach *.c |> gcc \$(CFLAGS) -c %f -o %o |> %B.o
: *.o |> gcc \$(LDFLAGS) %f -o %o |> prog
HERE

cat > Tuprules.tup << HERE
CFLAGS = -Wall
LDFLAGS = -lm
HERE
cat > fs/sub/Tuprules.tup << HERE
CFLAGS += -O0
HERE

tup touch fs/sub/Tupfile Tuprules.tup fs/sub/Tuprules.tup
tup touch fs/sub/helper.c
tup parse

tup_dep_exist fs/sub helper.c fs/sub 'gcc -Wall -O0 -c helper.c -o helper.o'
tup_dep_exist fs/sub helper.o fs/sub 'gcc -lm helper.o -o prog'

tup_dep_exist . Tuprules.tup fs sub
tup_dep_exist fs Tuprules.tup fs sub
tup_dep_exist fs/sub Tuprules.tup fs sub

cat > fs/Tuprules.tup << HERE
CFLAGS += -DFS=1
LDFLAGS += -lfoo
HERE
tup touch fs/Tuprules.tup
tup parse

tup_dep_exist fs/sub helper.c fs/sub 'gcc -Wall -DFS=1 -O0 -c helper.c -o helper.o'
tup_dep_exist fs/sub helper.o fs/sub 'gcc -lm -lfoo helper.o -o prog'

eotup
