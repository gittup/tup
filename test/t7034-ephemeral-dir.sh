#! /bin/sh -e

# Try to move a directory out of the way and then immediately move a new
# directory in its place. The monitor should still be running afterward, which
# is checked by stop_monitor.
. ./tup.sh
mkdir tuptest
cd tuptest
re_init
tup monitor

mkdir foo
cd foo
echo 'int main(void) {return 0;}' > foo.c
echo ': foreach *.c |> gcc %f -o %o |> %B' > Tupfile
cd ..
cp -Rp foo bar
update

mv foo ..
mv bar foo
stop_monitor

eotup
