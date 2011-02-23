#! /bin/sh -e

# This was discovered while trying to build ncurses. Because of the pattern of
# file accesses in a shell script, we end up with a file that has its modify
# flag set after a successful update. That's annoying.

. ./tup.sh
check_no_windows shell
cat > Tupfile << HERE
: |> echo hey > %o && ./foo.sh %o |> out.txt
HERE

# This script mimics something stupid done in ncurses' edit_cfg.sh
cat > foo.sh << HERE
mv \$1 tmp
cat tmp > \$1
rm tmp
HERE
chmod +x foo.sh
tup touch Tupfile foo.sh
update

eotup
