#! /bin/sh -e

# Make sure @(TUP_PLATFORM) has a default value, and can be overridden.

. ./tup.sh
cat > Tupfile << HERE
: |> echo @(TUP_PLATFORM) |>
HERE
tup touch Tupfile
tup parse

# Could validate other platforms here if desired - not really necessary.
if uname -s | grep Linux > /dev/null; then
	tup_object_exist . 'echo linux'
	tup_dep_exist @ TUP_PLATFORM 0 .
fi

varsetall TUP_PLATFORM=foo
tup parse
tup_object_exist . 'echo foo'
tup_dep_exist @ TUP_PLATFORM 0 .
tup_object_no_exist . 'echo linux'

eotup
