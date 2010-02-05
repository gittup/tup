#! /bin/sh -e

# Same as t5027, only the file we stomp is in a different directory.
. ./tup.sh

tmkdir sub
cat > sub/Tupfile << HERE
: |> echo 'foo' > %o |> file1
HERE
tup touch sub/Tupfile
update
echo 'foo' | diff - sub/file1

# Oops - accidentally overwrite file1
cat > Tupfile << HERE
: |> echo 'bar' > sub/file1 ; touch file2 |> file2
HERE
tup touch Tupfile
update_fail

cat > Tupfile << HERE
: |> echo 'bar' > %o |> file2
HERE
tup touch Tupfile
update
echo 'foo' | diff - sub/file1
echo 'bar' | diff - file2

eotup
