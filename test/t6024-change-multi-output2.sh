#! /bin/sh -e

# Similar to t6003, only we forget to update the shell script. Try to add a new
# target and see that the shell script runs again.

. ./tup.sh
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
: |> sh ok.sh |> a b c
HERE

tup touch Tupfile
update_fail

cat > ok.sh << HERE
touch a
touch b
touch c
HERE
tup touch ok.sh
update

check_exist a b c

eotup
