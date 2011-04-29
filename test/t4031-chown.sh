#! /bin/sh -e

# Try chown. Not sure how to really test this by changing the actual
# owner, or for running for people other than me.

. ./tup.sh
if ! whoami | grep marf > /dev/null; then
	echo "[33mSkip t4031 - you're not marf.[0m"
	eotup
fi

cat > Tupfile << HERE
: |> touch %o; chown marf:users %o |> test1
HERE
tup touch Tupfile
update

cat > Tupfile << HERE
: |> chown marf:users test2 |>
HERE
tup touch Tupfile test2
update_fail_msg "tup error.*chown"

eotup
