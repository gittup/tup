#! /bin/bash
rm -rf .run_test
mkdir .run_test
cd .run_test
niter=10
../gen-test-case.pl "$@" || exit 1
sync
cd tmake
find . -type f | while read i; do cat $i > /dev/null; done
echo "make: initial"
time -p make > /dev/null
sync
cfile=`find . -name 0.c`;
hfile=`find . -name 0.h`;

echo "make: 0.c touched"
for i in `seq 1 $niter`; do 
	sleep 1; touch $cfile
	time -p make > /dev/null
done

echo "make: 0.h touched"
for i in `seq 1 $niter`; do
	sleep 1; touch $hfile
	time -p make > /dev/null
done

echo "make: nothing"
for i in `seq 1 $niter`; do
	time -p make > /dev/null
done

cd ../ttup
find . -type f | while read i; do cat $i > /dev/null; done
tup init > /dev/null
echo "tup: initial"
tup monitor
sleep 1
find . -name Makefile -exec touch {} \;
time -p tup upd > /dev/null
sync
cfile=`find . -name 0.c`;
hfile=`find . -name 0.h`;

echo "tup: 0.c touched"
for i in `seq 1 $niter`; do
	touch $cfile
	time -p tup upd > /dev/null
done

echo "tup: 0.h touched"
for i in `seq 1 $niter`; do
	touch $hfile
	time -p tup upd > /dev/null
done

echo "tup: nothing"
for i in `seq 1 $niter`; do
	time -p tup upd > /dev/null
done

tup stop

cd ..
#diff -r tmake ttup | grep -v Makefile | grep -v build | grep -v '\.d$' | grep -v '\.tup'
