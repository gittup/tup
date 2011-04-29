#! /bin/sh -e

# We shouldn't delete user files if a command tries to write them. The command
# will be writing to temporary files anyway, not actually overwriting the
# user file.

. ./tup.sh

echo "hey there" > foo.c
cat > Tupfile << HERE
: |> touch foo.c |>
HERE
tup touch foo.c Tupfile
update_fail_msg "tup error.*utimens"

check_exist foo.c
echo "hey there" | diff - foo.c

eotup
