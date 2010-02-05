#! /bin/sh -e

# Make sure we can't include a Tupfile by using a generated symlink somewhere
# else in the path. Here we'll make a directory symlink in a rule, and then
# later try to include a Tupfile by using that directory link.

. ./tup.sh

tmkdir foo
tmkdir foo/arch-x86
echo 'var = 3' > foo/arch-x86/rules.tup
cat > foo/Tupfile << HERE
: arch-x86 |> ln -s %f %o |> arch
HERE
tup touch foo/arch-x86/rules.tup foo/Tupfile
update

cat > Tupfile << HERE
include foo/arch/rules.tup
HERE
tup touch Tupfile
update_fail

eotup
