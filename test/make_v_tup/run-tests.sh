#! /bin/sh
if cat /proc/sys/fs/inotify/max_user_watches | grep 65536 > /dev/null; then
	for i in 1 10 100 1000 10000; do ./run-test.sh -n $i > out-$i.txt 2>&1; done
else
	echo "Please set max_user_watches to 65536"
	exit 1;
fi
