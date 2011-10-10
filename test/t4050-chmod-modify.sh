#! /bin/sh -e

# Make sure that changing attributes counts as being modified.

. ./tup.sh

cat > ok.sh << HERE
echo foo
HERE
chmod +x ok.sh
cat > Tupfile << HERE
: |> ./ok.sh > %o |> bar
HERE
tup touch Tupfile ok.sh
update

sleep 1

chmod -x ok.sh
update_fail_msg "ok.sh.*Permission denied"

eotup
