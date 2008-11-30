#! /bin/sh -e

. ../tup.sh
tup monitor
cp ../testTupfile Tupfile

echo "int main(void) {return 0;}" > foo.c
update
tup_object_exist foo.o prog

# See if we can create and destroy a file almost immediately after and not have
# the monitor put the directory in create.
echo "Check 1"
touch bar.c
touch bar.c
touch bar.c
rm bar.c
check_empty_tupdirs

# See if we can do it when moving a file to a new file
echo "Check 2"
touch bar.c
mv bar.c newbar.c
rm newbar.c
check_empty_tupdirs
