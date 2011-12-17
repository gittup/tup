#! /bin/sh -e

# Try to run an external script to get :-rules in a sub-directory.
. ./tup.sh

tmkdir sub
cat > sub/gen.sh << HERE
#! /bin/sh
for i in *.c; do
	echo ": \$i |> gcc -c %f -o %o |> %B.o"
done
HERE
chmod +x sub/gen.sh
cat > sub/Tupfile << HERE
run ./gen.sh
HERE
tup touch sub/Tupfile sub/gen.sh sub/foo.c sub/bar.c
update

check_exist sub/foo.o sub/bar.o

# Now make sure we can just update the script and still get re-parsed.
cat > sub/gen.sh << HERE
#! /bin/sh
HERE
tup touch sub/gen.sh
update

check_not_exist sub/foo.o sub/bar.o
tup_dep_exist sub gen.sh . sub

# Now don't call gen.sh and make sure the dependency on the directory is gone.
cat > sub/Tupfile << HERE
HERE
tup touch sub/Tupfile
update

tup_dep_no_exist sub gen.sh . sub

eotup
