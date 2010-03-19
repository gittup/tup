#! /bin/sh -e

# Now try changing a variable and see the client re-execute

. ./tup.sh

make_tup_client

cat > Tupfile << HERE
: |> ./client defg > %o |> ok.txt
HERE
tup touch Tupfile empty.txt
update

diff empty.txt ok.txt

tup_object_exist @ defg

varsetall defg=hey
update

echo 'hey' | diff - ok.txt

eotup
