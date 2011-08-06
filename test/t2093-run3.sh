#! /bin/sh -e

# Execute a run-script from an included file.
. ./tup.sh

tmkdir sub
cat > sub/gen.sh << HERE
#! /bin/sh
for i in *.c; do
	echo ": \$i |> gcc -c %f -o %o |> %B.o"
done
HERE
chmod +x sub/gen.sh

cat > sub/inc.tup << HERE
run \$(TUP_CWD)/gen.sh
HERE
cat > Tupfile << HERE
include sub/inc.tup
HERE
tup touch Tupfile sub/inc.tup sub/gen.sh foo.c bar.c
update

check_exist foo.o bar.o

eotup
