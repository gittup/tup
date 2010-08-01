#! /bin/sh -e

# Make sure that when one of the links we are relying on for transitive deps
# goes away, we get an error message.
# eg: a -> b -> c and a -> c
# then we remove a -> b, so the a -> c link should get an error


. ./tup.sh

cat > ok1.sh << HERE
cat foo
HERE

cat > ok2.sh << HERE
cat foo
cat bar
HERE

chmod +x ok1.sh ok2.sh

cat > Tupfile << HERE
: |> echo blah > %o |> foo
: foo |> ./ok1.sh > %o |> bar
: bar |> ./ok2.sh |>
HERE
update

cat > ok1.sh << HERE
echo blah
HERE
tup touch ok1.sh
update_fail_msg "Missing input dependency"

eotup
