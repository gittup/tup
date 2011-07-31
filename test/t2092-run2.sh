#! /bin/sh -e

# Try to wildcard directories from the run-script. This should cause a
# directory-level dependency in the parser.
. ./tup.sh

cat > gen.sh << HERE
#! /bin/sh
for i in *.c src/*.c; do
	echo ": \$i |> gcc -c %f -o %o |> %B.o"
done
HERE
chmod +x gen.sh
cat > Tupfile << HERE
run ./gen.sh
HERE
tmkdir src
tup touch Tupfile gen.sh foo.c
update

check_exist foo.o

# Now make sure that when we create a new file in the src/ directory, the
# top-level directory gets re-parsed and baz.o is created.
tup touch src/bar.c
update

check_exist bar.o

eotup
