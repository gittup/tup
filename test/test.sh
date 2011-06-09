#! /bin/sh -e
#
# Use as:
# To run a single test: ./test.sh t0000-init.sh
# To run all tests: ./test.sh
# To run all tests without stopping on errors: ./test.sh --keep-going
#
# You can also invoke a test case directly (as in: ./t0000-init.sh), but if it
# fails you may not always get an error message. You can still tell if the test
# failed by checking the return code (non-zero), and the tuptesttmp directory
# for that test will remain for manual inspection.

keep_going=0
while [ $# -gt 0 ]; do
	if [ "$1" = "-k" ]; then
		keep_going=1
	elif [ "$1" = "--keep-going" ]; then
		keep_going=1
	else
		files="$files $1"
	fi
	shift
done

if [ "$files" = "" ]; then
	files="t[0-9]*.sh"
fi

for i in $files; do
	echo "[36m --- Run $i --- [0m"
	if ./$i; then
		:
	else
		echo "[31m *** $i failed[0m"
		if [ "$keep_going" = "0" ]; then
			exit 1
		fi
	fi
done
