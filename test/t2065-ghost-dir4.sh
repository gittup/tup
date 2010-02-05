#! /bin/sh -e

# Like t2040, but now with a longer chain of ghosts.

. ./tup.sh
cat > ok.sh << HERE
cat super/secret/ghost 2>/dev/null || echo nofile
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
chmod +x ok.sh
tup touch ok.sh Tupfile
update
tup_object_exist . super
tup_object_exist super secret
tup_object_exist super/secret ghost

rm -f Tupfile
tup rm Tupfile
update

tup_object_no_exist . super
tup_object_no_exist super secret
tup_object_no_exist super/secret ghost

eotup
