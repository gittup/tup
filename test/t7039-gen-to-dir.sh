#! /bin/sh -e

# Similar bug to t7038 - when the files are removed by the update process, the
# tupids are still cached by the tup monitor process, so when the directory is
# created the monitor gets wonky and dies.

. ./tup.sh
check_monitor_supported

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
update
check_not_exist ok.o open.o prog
mkdir foo
cp ok.c foo
cat > foo/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
HERE
update

check_exist foo/prog
stop_monitor

eotup
