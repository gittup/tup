#! /bin/sh -e

# After creating a ghost dir and ghost node, remove the rule and see that both
# ghosts are poofed.

. ../tup.sh
cat > ok.sh << HERE
cat secret/ghost 2>/dev/null || echo nofile
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
chmod +x ok.sh
tup touch ok.sh Tupfile
update
tup_object_exist . secret
tup_object_exist secret ghost

rm -f Tupfile
tup rm Tupfile
update

tup_object_no_exist . secret
tup_object_no_exist secret ghost
