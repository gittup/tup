#! /bin/sh -e

# This was found while trying to compile ncurses. A shell script is executed to
# generate a header file. The script creates some temporary files (so they show
# up as outputs to the command), but the files are then removed. However, they
# stick around as outputs, so the next time the Tupfile is modified, they end
# up getting moved over to delete. Either way, those temporary files shouldn't
# show up in the tup db (basically, these are ephemeral files, only instead of
# the monitor they come in from the ldpreload/wrapper).
. ../tup.sh
cat > Tupfile << HERE
: |> ./foo.sh |> foo.h
HERE

cat > foo.sh << HERE
# This trap business mimics ncurses' MKkey_defs.sh. All that really matters is
# the foo.tmp file is removed before we leave
trap 'rm -f foo.tmp' 0 1 2 5 15
echo "int foo(void);" > foo.tmp
cat foo.tmp > foo.h
HERE
chmod +x foo.sh

tup touch Tupfile foo.sh
update
check_exist foo.h
check_not_exist foo.tmp
tup_object_exist . foo.h
tup_object_no_exist . foo.tmp
