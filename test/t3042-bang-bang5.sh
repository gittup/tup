#! /bin/sh -e

# See if we can override a bang macro using an alias

. ./tup.sh
cat > Tupfile << HERE
!touch = |> touch %o |>
!cc = |> gcc -c %f -o %o |>

!mplayer.c = !touch
!mplayer.asm = !touch

# Now override
!mplayer.c = !cc

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
