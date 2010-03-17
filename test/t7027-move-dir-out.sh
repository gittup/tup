#! /bin/sh -e

# Make sure we can move a directory outside of tup and have it be deleted.

. ./tup.sh
check_monitor_supported
mkdir tuptest
cd tuptest
re_init
tup monitor

mkdir foo
cd foo
echo 'int main(void) {return 0;}' > foo.c
echo ': foreach *.c |> gcc %f -o %o |> %B' > Tupfile
cd ..
update

mv foo ..
stop_monitor
update
tup_object_no_exist . foo

eotup
