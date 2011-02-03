#! /bin/bash

TUP=tup

rm -rf .run_test
mkdir .run_test
cd .run_test
niter=3
../gen-test-case.pl "$@" || exit 1
sync

# Run the full gamut of tests for one of the tools:
benchmark ()
{
	tool="$1"
	initialize="$2"
	start="$3"
	update="$4"
	finish="$5"

	cd t$tool
	find . -type f | while read i; do cat $i > /dev/null; done
	eval "$initialize"
	echo "$tool: initial"
	eval "$start"
	time -p eval "$update"
	sync
	cfile=`find . -name 0.c`;
	hfile=`find . -name 0.h`;

	echo "$tool: 0.c touched"
	for i in `seq 1 $niter`; do
		sleep 1; touch $cfile
		time -p eval "$update"
	done

	echo "$tool: 0.h touched"
	for i in `seq 1 $niter`; do
		sleep 1; touch $hfile
		time -p eval "$update"
	done

	echo "$tool: nothing"
	for i in `seq 1 $niter`; do
		time -p eval "$update"
	done

	eval "$finish"
	cd ..
}

benchmark "make" ":" ":" "make -rR > /dev/null" ":"
benchmark "tup" "$TUP init --force > /dev/null" "$TUP monitor" "$TUP upd > /dev/null" "$TUP stop"

#diff -r tmake ttup | grep -v Makefile | grep -v build | grep -v '\.d$' | grep -v '\.tup'
