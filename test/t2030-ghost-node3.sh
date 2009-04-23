#! /bin/sh -e

# Make sure if we have a ghost node that it doesn't get used in wildcarding or
# explicit inputs.

. ../tup.sh
cat > ok.sh << HERE
if [ -f ghost ]; then cat ghost; else echo nofile; fi
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
chmod +x ok.sh
tup touch ok.sh Tupfile
update
tup_object_exist . ghost
echo nofile | diff output.txt -

# This should parse correctly because the g* shouldn't match the ghost node
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
: foreach g* |> cat %f |>
HERE
tup touch Tupfile
tup parse
tup_object_no_exist . 'cat ghost'
update

# When we explicitly name the file, it should fail
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
: ghost |> cat %f |>
HERE
tup touch Tupfile
parse_fail "Shouldn't be able to parse the Tupfile with a ghost node as input."
