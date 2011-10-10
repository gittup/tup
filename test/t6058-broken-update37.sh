#! /bin/sh -e

# If we create two files with the same timestamps, then move one out of the way
# and rename the first over the second, the file contents are effectively
# changed (since file 'b' has the contents of 'a'), but the mtime is unchanged
# since mv doesn't update it. Now running tup won't actually update any files
# dependent on 'b', even though they would have changed if running from
# scratch.

. ./tup.sh

if [ ! "$tupos" = "Linux" ]; then
	echo "[33mTODO: mv only updates ctime on linux. Skipping test.[0m"
	eotup
fi


echo 'this is A' > a.txt
echo 'this is B' > b.txt
cat > Tupfile << HERE
: foreach *.txt |> cp %f %o |> %B.out
HERE
tup touch Tupfile a.txt b.txt
update

echo 'this is A' | diff - a.out
echo 'this is B' | diff - b.out

sleep 1

mv b.txt c.txt
mv a.txt b.txt
update
echo 'this is A' | diff - b.out
echo 'this is B' | diff - c.out

eotup
