#! /bin/sh
# This script is used to see how various parts of tup scale, or how tup changes
# between versions.
#
# Run all benchmarks: ./bench.sh
# Run all benchmarks with 1000 iterations: ./bench.sh NUM=1000
# Run specific benchmarks: ./bench.sh b00-init.sh
#
# Normally I might run just ./bench.sh with different checkouts of tup. Or, I
# might run ./bench.sh with NUM=100, 1000, 10000, etc to see how it scales.

NUM=100

while [ $# -gt 0 ]; do
	if echo $1 | grep 'NUM=' > /dev/null; then
		NUM=`echo $1 | sed 's/NUM=//'`
	else
		files="$files $1"
	fi
	shift
done
if [ "$files" == "" ]; then
	files="b[0-9]*.sh"
fi

echo "" > tupbenchtmps.t
echo "" > tupbenchtmps.d

for i in $files; do
	echo -n "[36m Bench[$i]:[0m	"
	testdir="tupbenchtmp-$i"
	rm -rf $testdir
	mkdir $testdir
	cd $testdir
	t=`((time -p (tup init --force > /dev/null; ../$i $NUM > /dev/null 2>&3); echo -n MARF$?FRAM 1>&4) 2>&1 | grep ^real | awk '{print $2}') 3>&1 4>&1 | sed 's/MARF0FRAM//'`
	if echo "$t" | grep 'MARF' > /dev/null; then
		echo "$t"
		exit 1
	fi
	d=`du -sk .tup | awk '{print $1}'`
	echo ${t}s ${d}k
	cd ..
	rm -rf $testdir
	echo $t >> tupbenchtmps.t
	echo $d >> tupbenchtmps.d
done

echo " ----------------------------------"
echo -n "[36m Summary:[0m		"
t=`cat tupbenchtmps.t | awk '{x+=$1} END{print x}'`
d=`cat tupbenchtmps.d | awk '{x+=$1} END{print x}'`
echo "${t}s ${d}k"
rm -f tupbenchtmps.[td]
