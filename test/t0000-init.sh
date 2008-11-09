#! /bin/sh -e

for i in db object updater; do
	if [ ! -f ".tup/$i" ]; then
		echo ".tup/$i not created!" 1>&2
		exit 1
	fi
done
