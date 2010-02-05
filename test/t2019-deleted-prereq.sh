#! /bin/sh -e

# Like t2011 and t2018, but this time the prerequisite gets deleted later.

. ./tup.sh
cat > Tupfile << HERE
: foo.txt |> cat %f > %o |> bar.txt
: bar.txt |> cat %f |>
HERE

echo hey > foo.txt
tup touch Tupfile foo.txt
update

cat > Tupfile << HERE
: bar.txt |> cat %f |>
HERE
tup touch Tupfile
update_fail

eotup
