#! /bin/sh -e

# Try to access a subdirectory from the run-script. This should cause a
# directory-level dependency in the parser.
. ./tup.sh

cat > gen.sh << HERE
#! /bin/sh
for i in *.c src/bar.c; do
	echo ": \$i |> gcc -c %f -o %o |> %B.o"
done
HERE
chmod +x gen.sh
cat > Tupfile << HERE
run ./gen.sh
HERE
tmkdir src
tup touch Tupfile gen.sh foo.c src/bar.c
update

check_exist foo.o bar.o
tup_dep_exist . src 0 .

eotup
