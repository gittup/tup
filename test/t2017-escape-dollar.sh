#! /bin/sh -e

# See if we can escape a dollar-sign paren

. ../tup.sh
cat > Tupfile << HERE
: |> FOO="hey"; export FOO; echo \$FOO |>
: |> echo \\\$(ls) |>
HERE
tup touch Tupfile
tup upd
tup_object_exist . 'FOO="hey"; export FOO; echo $FOO'
tup_object_exist . 'echo $(ls)'
