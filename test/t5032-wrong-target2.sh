#! /bin/sh -e

# Similar to t5027, only this time we write a symlink in the wrong spot. This
# is necessary in addition to t5027 because symlinks are handled differently
# than normal output files.
. ../tup.sh

cat > Tupfile << HERE
: |> echo 'foo' > %o |> file1
HERE
tup touch Tupfile
update
echo 'foo' | diff - file1

# Oops - accidentally overwrite file1 with a symlink
cat > Tupfile << HERE
: |> echo 'foo' > %o |> file1
: |> touch file2; ln -sf file2 file1 |> file2
HERE
tup touch Tupfile
update_fail

# The echo 'foo' > file1 command should run again. Note that file1 was a
# symlink to file2, but tup rm file1 so the command should succeed again.
cat > Tupfile << HERE
: |> echo 'foo' > %o |> file1
: |> ln -s file1 %o |> file2
HERE
tup touch Tupfile
update
echo 'foo' | diff - file1
