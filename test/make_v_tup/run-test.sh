#! /bin/bash
rm -rf .run_test
mkdir .run_test
cd .run_test
niter=3
../gen-test-case.pl "$@" || exit 1
sync

tool="make"
update="make -rR > /dev/null"
cd t$tool
find . -type f | while read i; do cat $i > /dev/null; done
echo "$tool: initial"
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
cd ..

tool="tup"
update="tup upd > /dev/null"
cd t$tool
find . -type f | while read i; do cat $i > /dev/null; done
tup init --force > /dev/null
echo "$tool: initial"
tup monitor
time -p eval "$update"
sync
cfile=`find . -name 0.c`;
hfile=`find . -name 0.h`;

echo "$tool: 0.c touched"
for i in `seq 1 $niter`; do
	touch $cfile
	time -p eval "$update"
done

echo "$tool: 0.h touched"
for i in `seq 1 $niter`; do
	touch $hfile
	time -p eval "$update"
done

echo "$tool: nothing"
for i in `seq 1 $niter`; do
	time -p eval "$update"
done

tup stop
cd ..

#diff -r tmake ttup | grep -v Makefile | grep -v build | grep -v '\.d$' | grep -v '\.tup'
