#! /bin/sh -e

# When the parser version updates, make sure all Tupfiles are re-parsed.

. ./tup.sh

cat > Tupfile << HERE
: |> touch %o |> foo
HERE
tmkdir sub
cat > sub/Tupfile << HERE
: |> touch %o |> bar
HERE
tup touch Tupfile sub/Tupfile
update

check_exist foo sub/bar
tup fake_parser_version

cat > Tupfile << HERE
: |> touch %o |> foo
: |> touch %o |> foo2
HERE
cat > sub/Tupfile << HERE
: |> touch %o |> bar
: |> touch %o |> bar2
HERE

# Don't tup touch the Tupfiles - the parser version should cause them to update
update > .tupoutput
if ! grep 'Tup parser version has been updated' .tupoutput > /dev/null; then
	echo "*** Expected the parser version update message to be displayed, but it wasn't." 1>&2
	exit 1
fi
check_exist foo2 sub/bar2

eotup
