#! /bin/sh -e

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

cat > ok.sh << HERE
touch a
touch c
HERE

cat > Tupfile << HERE
: |> sh ok.sh |> a c
HERE

tup touch ok.sh Tupfile
update

check_exist a c
check_not_exist b

eotup
