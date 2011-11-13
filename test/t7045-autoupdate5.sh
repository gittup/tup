#! /bin/sh -e

# Make sure we do an initial update if autoupdate is requested. Otherwise some
# files may be modified, and you start the monitor, but you still have to
# manually 'tup upd' once or touch a random file to trigger the update.

. ./tup.sh
check_monitor_supported
tup monitor --autoupdate
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
echo 'int foo(void) {return 7;}' > ok.c

tup flush
sym_check ok.o foo

tup stop
sleep 1
echo 'int bar(void) {return 6;}' > ok.c

tup monitor --autoupdate

tup flush
sym_check ok.o ^foo bar

eotup
