#! /bin/sh -e

. ../tup.sh
echo 'this is a file' > file1
ln file1 file2
cat > Makefile << HERE
all: new-file1 new-file2
new-%: %
	create_dep "tup wrap cp \$< \$@" -i\$< -o\$@
HERE

tup startmon
update
check_exist new-file1 new-file2

rm new-file1 new-file2
update

touch file1
tup stopmon

update
check_exist new-file1 new-file2
