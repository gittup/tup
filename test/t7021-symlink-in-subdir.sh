#! /bin/sh -e

# While moving things around in the monitor, I found out that I don't have a
# test to check for a symlink working properly in a subdir. (By moving things
# around I found out I was relying on the cwd to be set for readlink() to work
# in update_symlink_file). This makes sure I don't break that again.
. ../tup.sh

mkdir a
cd a
touch foo
ln -s foo bar
tup monitor
tup flush
tup_object_exist a foo
tup_object_exist a bar
