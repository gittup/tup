#! /bin/sh -e

# See if we can escape a dollar-sign

. ../tup.sh
cat > Tupfile << HERE
: |> FOO="hey"; export FOO; echo \\\$FOO |>
HERE
tup touch Tupfile
tup upd
tup_object_exist . 'FOO="hey"; export FOO; echo $FOO'
