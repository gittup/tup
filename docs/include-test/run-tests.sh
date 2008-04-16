#! /bin/sh
for i in 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536; do
	./run-test.sh $i
	x=`(time -p make > /dev/null) 2>&1 | grep real | sed 's/real //'`
	echo "$i $x"
done
