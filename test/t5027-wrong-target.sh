#! /bin/sh -e

# If we have two commands, they obviously have to write to different files.
# However, if one of them behaves poorly and overwrites the other guy's output,
# we should automatically re-run the first command.
. ../tup.sh

cat > Tupfile << HERE
: |> echo 'foo' > %o |> file1
HERE
tup touch Tupfile
update
echo 'foo' | diff - file1

# Oops - accidentally overwrite file1
cat > Tupfile << HERE
: |> echo 'foo' > %o |> file1
: |> echo 'bar' > file1 ; touch file2 |> file2
HERE
tup touch Tupfile
update_fail

cat > Tupfile << HERE
: |> echo 'foo' > %o |> file1
: |> echo 'bar' > %o |> file2
HERE
tup touch Tupfile
update
echo 'foo' | diff - file1
echo 'bar' | diff - file2
