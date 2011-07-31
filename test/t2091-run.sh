#! /bin/sh -e

# Try to run an external script to get :-rules.
. ./tup.sh

cat > gen.sh << HERE
#! /bin/sh
for i in *.c; do
	echo ": \$i |> gcc -c %f -o %o |> %B.o"
done
HERE
chmod +x gen.sh
cat > Tupfile << HERE
run ./gen.sh
HERE
tup touch Tupfile gen.sh foo.c bar.c
update

check_exist foo.o bar.o

# Now make sure we can just update the script and still get re-parsed.
cat > gen.sh << HERE
#! /bin/sh
HERE
tup touch gen.sh
update

check_not_exist foo.o bar.o
tup_dep_exist . gen.sh 0 .

# Now don't call gen.sh and make sure the dependency on the directory is gone.
cat > Tupfile << HERE
HERE
tup touch Tupfile
update

tup_dep_no_exist . gen.sh 0 .

eotup
