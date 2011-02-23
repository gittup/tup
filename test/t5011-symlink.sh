#! /bin/sh -e

. ./tup.sh
check_no_windows symlink
echo 'this is a file' > file1
cat > Tupfile << HERE
: file1 |> ln -s %f %o |> file1.sym
HERE
tup touch file1 Tupfile
update

check_exist file1.sym
cat > Tupfile << HERE
HERE
tup touch Tupfile
update
check_not_exist file1.sym

eotup
