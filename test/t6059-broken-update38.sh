#! /bin/sh -e

# Including an empty file name shouldn't segfault. This can happen if we
# expand a variable to an empty string, as below.

. ./tup.sh

cat > Tupfile << HERE
var=
include foo
include \$(var)
HERE
tup touch Tupfile foo
parse_fail_msg 'Invalid include filename'

eotup
