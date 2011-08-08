#! /bin/sh -e

# Make sure only outputs before the run-script is executed are seen
# by readdir(). Otherwise, both the fuse thread and the parser thread
# could be accessing the database at the same time.
. ./tup.sh

cat > Tupfile << HERE
run sh -e ok.sh
HERE
cat > ok.sh << HERE
echo ": |> touch %o |> foo.c"
for i in *.c; do
	echo ": \$i|> gcc -c %f -o %o |> %B.o"
done
HERE
tup touch Tupfile ok.sh bar.c
update

check_exist bar.o
check_exist foo.c
check_not_exist foo.o

eotup
