#! /bin/sh -e

# Try a run-script with PATH
. ./tup.sh

export PATH=$PWD/a:$PATH

tmkdir a
cat > a/gen.sh << HERE
#! /bin/sh
for i in *.c; do
	echo ": \$i |> gcc -c %f -o %o |> %B.o"
done
HERE

tmkdir b
cat > b/gen.sh << HERE
#! /bin/sh
for i in *.c; do
	echo ": \$i |> gcc -Wall -c %f -o %o |> %B.o"
done
HERE
chmod +x a/gen.sh b/gen.sh
cat > Tupfile << HERE
HERE
tup touch Tupfile a/gen.sh b/gen.sh foo.c bar.c
update

# We should only get the directory-level dependency on PATH when we actually
# execute a run-script
tup_dep_no_exist $ PATH 0 .

cat > Tupfile << HERE
run gen.sh
HERE
tup touch Tupfile
update

tup_dep_exist $ PATH 0 .

check_exist foo.o bar.o
tup_object_exist . 'gcc -c foo.c -o foo.o'
tup_object_exist . 'gcc -c bar.c -o bar.o'

# Now just changing the PATH should cause the Tupfile to be re-parsed, resulting
# in the new run script to execute.
export PATH=$PWD/b:$PATH
update
tup_object_exist . 'gcc -Wall -c foo.c -o foo.o'
tup_object_exist . 'gcc -Wall -c bar.c -o bar.o'

eotup
