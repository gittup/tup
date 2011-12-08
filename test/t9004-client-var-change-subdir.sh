#! /bin/sh -e

# Now try changing a variable and see the client re-execute

. ./tup.sh
check_no_windows client

make_tup_client
tmkdir sub
cd sub
mv ../client .

cat > Tupfile << HERE
: |> ./client defg > %o |> ok.txt
HERE
tup touch Tupfile empty.txt
update

diff empty.txt ok.txt

tup_object_exist @ defg

cd ..
varsetall defg=hey
cd sub
update

echo 'hey' | diff - ok.txt

eotup
