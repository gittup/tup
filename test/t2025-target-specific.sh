#! /bin/sh -e

# See if target specific variables work. Also make sure we can't post-set
# variables after a rule.

. ../tup.sh
cat > Tupfile << HERE
var_foo = hey
var_bar = yo
srcs = *.c
: foreach \$(srcs) |> gcc -DBLAH=\$(var_%B) -c %f -o %o |> %B.o
var_bar = BREAK
HERE

tup touch Tupfile foo.c bar.c
tup parse
tup_object_exist . "gcc -DBLAH=hey -c foo.c -o foo.o"
tup_object_exist . "gcc -DBLAH=yo -c bar.c -o bar.o"
