#! /bin/sh -e

# TODO

. ../tup.sh
tmkdir foo
tmkdir include
cat > foo/Tupfile << HERE
: |> if [ -f ../include/../../../../../../../bar/lg.h ]; then echo yay; else echo no; fi |>
HERE
tup touch foo/Tupfile
tup upd
