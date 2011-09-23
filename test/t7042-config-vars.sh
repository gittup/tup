#! /bin/sh -e

# Make sure we can read in config vars with the monitor running.

. ./tup.sh
check_monitor_supported
tup monitor
cat > Tupfile << HERE
srcs-@(FOO) += foo.c
srcs-@(BAR) += bar.c
: foreach \$(srcs-y) |> gcc -c %f -o %o |> %B.o
HERE

touch foo.c bar.c
update
check_not_exist foo.o bar.o

varsetall FOO=y
update
check_not_exist bar.o
check_exist foo.o

varsetall BAR=y
update
check_not_exist foo.o
check_exist bar.o

eotup
