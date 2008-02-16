export LD_PRELOAD=/home/mjs/tup/ldpreload/ldpreload.so
strace -f gcc -c main.c
unset LD_PRELOAD
