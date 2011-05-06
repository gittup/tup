#! /bin/sh -e

# With find_existing_command(), we try to rename a command node to a new name
# if we find that the output already has a command pointing to it (ie:
# we change 'gcc -c foo.c' to 'gcc -W -c foo.c' or something). This would just
# try to set the name on the node. However, if a node named 'gcc -W -c foo.c'
# already existed, then we'd get an SQL error because we would violate the
# unique constraint.
#
# In the test below, we have a './ok.sh' command node, and the touch command
# points to the file 'c'. Then we try to change the command that points to
# 'c' to be './ok.sh', so the rename fails. That shouldn't happen and instead
# we can just keep using the existing './ok.sh' and update the links
# appropriately.

. ./tup.sh

cat > ok.sh << HERE
cat a
touch b
HERE
chmod +x ok.sh
cat > Tupfile << HERE
: a |> ./ok.sh |> b
: |> touch %o |> c
HERE
tup touch a ok.sh Tupfile
update

cat > ok.sh << HERE
cat a
touch c
HERE
cat > Tupfile << HERE
: a |> ./ok.sh |> c
HERE
tup touch Tupfile
update

eotup
