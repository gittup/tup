#! /bin/sh -e

echo "[33mSkip t5005 - not needed?[0m"
exit 0

. ../tup.sh
echo 'this is a file' > file1
ln file1 file2
cat > Makefile << HERE
all: new-file1 new-file2
new-%: %
	tup link "tup wrap cp \$< \$@" -i\$< -o\$@
HERE

tup touch file1 file2 Makefile
update
check_exist new-file1 new-file2

rm new-file1 new-file2

tup touch file1

update
check_exist new-file1 new-file2
