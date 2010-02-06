#! /bin/sh -e

# See if we can set a bang macro equal to a previous macro

. ./tup.sh
cat > Tupfile << HERE
!cc = |> gcc -c %f -o %o |>

!mplayer.c = !cc
!mplayer.asm = |> touch %o |>

files += foo.c
files += bar.asm
: foreach \$(files) |> !mplayer |> %B.o
HERE
tup touch foo.c bar.asm Tupfile
update

check_exist foo.o bar.o
tup_dep_exist . foo.c . 'gcc -c foo.c -o foo.o'
tup_dep_exist . bar.asm . 'touch bar.o'

eotup
