#! /bin/sh -e

# See if we can use backslashes in sed without having to escape them.
#
# Note that I have to escape them in the shell here. In reality the Tupfile
# looks like:
#
# : |> echo 'foo$' | sed 's/o\$/3/' > %o |> out.txt
#
# Which matches how we would write it on the command line (the '\$' in the sed
# command is supposed to match the literal '$' in 'foo$').

. ../tup.sh
cat > Tupfile << HERE
: |> echo 'foo\$' | sed 's/o\\\$/3/' > %o |> out.txt
HERE
cat Tupfile
tup touch Tupfile
update
echo 'fo3' | diff - out.txt
