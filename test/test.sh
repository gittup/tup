#! /bin/sh -e
#
# Use as:
# To run a single test: ./test.sh t0000-init.sh
# To run all tests: ./test.sh
#
# You can also invoke a test case directly (as in: ./t0000-init.sh), but if it
# fails you may not always get an error message. You can still tell if the test
# failed by checking the return code (non-zero), and the tuptesttmp directory
# for that test will remain for manual inspection.

files=$@
if [ "$files" == "" ]; then
	files="t[0-9]*.sh"
fi

for i in $files; do
	echo "[36m --- Run $i --- [0m"
	if ./$i; then
		:
	else
		echo "[31m *** $i failed[0m"
		exit 1
	fi
done
