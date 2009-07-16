#! /bin/sh -e

# Similar to t6003, only we forget to update the shell script. The command
# should still try to run again, so that we can find out that it is writing to
# the wrong files.

. ../tup.sh
cat > Tupfile << HERE
: |> sh ok.sh |> a b
HERE

cat > ok.sh << HERE
touch a
touch b
HERE

tup touch ok.sh Tupfile
update
check_exist a b
check_not_exist c

cat > Tupfile << HERE
: |> sh ok.sh |> a c
HERE

tup touch Tupfile
update_fail

cat > ok.sh << HERE
touch a
touch c
HERE
tup touch ok.sh
update

check_exist a c
check_not_exist b
