#! /bin/sh -e

# Make sure a ghost node from a previous command doesn't affect a new command
# that doesn't rely on its ghostly past.

. ./tup.sh
cat > ok.sh << HERE
if [ -f ghost ]; then cat ghost; else echo nofile; fi
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
chmod +x ok.sh
tup touch ok.sh Tupfile
update
echo nofile | diff output.txt -
tup_dep_exist . ghost . './ok.sh > output.txt'

# Change ok.sh so it doesn't try to read from ghost, and make sure the
# dependency is gone.
cat > ok.sh << HERE
echo nofile
HERE
tup touch ok.sh
update
tup_object_no_exist . ghost

# Just as a double-check of sorts - actually create the ghost node and update,
# after deleting output.txt from behind tup's back. The output.txt file
# shouldn't be re-created (as it would be in t2028).
echo 'hey' > ghost
tup touch ghost
rm -f output.txt
update --no-scan
check_not_exist output.txt

eotup
