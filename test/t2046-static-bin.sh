#! /bin/sh -e

# Check that static binning (using braces to dump output files into a bin)
# works.

. ../tup.sh
cat > Tupfile << HERE
: foreach foo.c bar.c |> gcc -c %f -o %o |> %B.o {objs}
: {objs} |> gcc %f -o %o |> prog
: *.o |> test %f |>
HERE
tup touch foo.c bar.c Tupfile
tup parse
tup_dep_exist . 'foo.o' . 'gcc foo.o bar.o -o prog'
tup_dep_exist . 'bar.o' . 'gcc foo.o bar.o -o prog'
tup_dep_exist . 'foo.o' . 'test bar.o foo.o'
tup_dep_exist . 'bar.o' . 'test bar.o foo.o'
