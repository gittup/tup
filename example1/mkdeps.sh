#! /bin/sh
txt=
for i in `seq 1 300`; do txt="$txt $i.h"; done
for j in 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384; do 
	for i in `seq 1 $j`; do
		echo "all: $i.o" > $i.d
		echo "$i.o: $txt ; touch \$@" >> $i.d
	done
	make
	echo -n $j >> output.txt
	(time make) 2>&1 | grep ^real | sed 's/real *//' >> output.txt
done
