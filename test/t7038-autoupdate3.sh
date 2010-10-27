#! /bin/sh -e

# Here we create a program so we have some generated files, then start up the
# monitor with autoupdate on. When the Tupfile is changed to no longer create
# the files, they are removed by the update. The monitor still has their
# entries cached, however, so when we try to add new generated files, the
# tupids get re-used, but the monitor can't insert them into the tree since it
# thinks they are already there.

. ./tup.sh
check_monitor_supported
tup config autoupdate 1

echo "int main(void) {return 0;}" > ok.c
touch open.c
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
HERE
update
check_exist prog

tup monitor

cat > Tupfile << HERE
HERE
tup flush
check_not_exist ok.o open.o prog

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
HERE
tup flush
check_exist prog

stop_monitor
eotup
