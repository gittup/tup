#! /bin/sh -e

# Make sure a sub-process only has the minimum necessary fds open.
. ./tup.sh

if [ ! "$tupos" = "Linux" ]; then
	echo "Sub-process fds only checked under linux. Skipping test."
	eotup
fi
cat > Tupfile << HERE
: |> ls -l /proc/\$\$/fd > %o |> fds.txt
HERE
tup touch Tupfile
update

text=`cat fds.txt | grep -v ' 0 .*/dev/null' | grep -v ' 1 .*output-' | grep -v ' 2 .*errors' | grep -v '/\.tup/vardict'`
if [ "$text" != "total 0" ]; then
	echo "Error: These fds shouldn't be open: $text" 1>&2
	exit 1
fi

eotup
