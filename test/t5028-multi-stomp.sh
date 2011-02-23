#! /bin/sh -e

# Stomp on multiple other files.
. ./tup.sh

cat > Tupfile << HERE
: |> echo foo > %o |> file1
: |> echo foo2 > %o |> file2
HERE
tup touch Tupfile
update
echo foo | diff - file1
echo foo2 | diff - file2

# Stomp stomp stomp
cat > Tupfile << HERE
: |> echo foo > %o |> file1
: |> echo foo2 > %o |> file2
: |> echo bar > file1 ; echo bar2 > file2; touch file3 |> file3
HERE
tup touch Tupfile
update_fail

cat > Tupfile << HERE
: |> echo foo > %o |> file1
: |> echo foo2 > %o |> file2
: |> echo bar > %o |> file3
HERE
tup touch Tupfile
update
echo foo | diff - file1
echo foo2 | diff - file2
echo bar | diff - file3

eotup
