#! /bin/sh -e

# Make sure moving a directory out of tup will successfully remove watches on
# all the subdirectories. Use the USR1 signal to have the monitor quit if it
# has a watch on any invalid tupid.

. ./tup.sh
mkdir tuptest
cd tuptest
re_init
tup monitor

mkdir -p foo/bar
cd foo/bar
echo 'int main(void) {return 0;}' > foo.c
echo ': foreach *.c |> gcc %f -o %o |> %B' > Tupfile
cd ../..
update

signal_monitor

mv foo ..
tup flush
signal_monitor
stop_monitor
update
tup_object_no_exist . foo

eotup
