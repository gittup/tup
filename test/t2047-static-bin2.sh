#! /bin/sh -e

# See if we can use separate rules to go into the same bin. Note I have no idea
# if the syntax for 'as' is correct - I'm just checking the parser.

. ../tup.sh
cat > Tupfile << HERE
: foo.c |> gcc -c %f -o %o |> %B.o [objs]
: bar.S |> as %f -o %o |> %B.o [objs]
: [objs] |> gcc %f -o %o |> prog
HERE
tup touch foo.c bar.S Tupfile
tup parse
tup_dep_exist . 'foo.o' . 'gcc foo.o bar.o -o prog'
tup_dep_exist . 'bar.o' . 'gcc foo.o bar.o -o prog'

# Re-order the first two rules.
cat > Tupfile << HERE
: bar.S |> as %f -o %o |> %B.o [objs]
: foo.c |> gcc -c %f -o %o |> %B.o [objs]
: [objs] |> gcc %f -o %o |> prog
HERE
tup touch Tupfile
# Parse and delete here to remove the old command
tup delete
tup_dep_no_exist . 'foo.o' . 'gcc foo.o bar.o -o prog'
tup_dep_no_exist . 'bar.o' . 'gcc foo.o bar.o -o prog'
tup_dep_exist . 'foo.o' . 'gcc bar.o foo.o -o prog'
tup_dep_exist . 'bar.o' . 'gcc bar.o foo.o -o prog'
