#! /bin/sh -e

# Try to use the .gitignore directive

. ../tup.sh
cat > Tuprules.tup << HERE
.gitignore
HERE

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
include_rules
HERE
echo 'int main(void) {return 0;}' > foo.c

tmkdir sub
cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> ar cr %o %f |> libsub.a
include_rules
HERE

tup touch foo.c bar.c Tupfile Tuprules.tup sub/Tupfile sub/shazam.c
update

if [ ! -f .gitignore ]; then
	echo "Error: .gitignore file not generated" 1>&2
	exit 1
fi
if [ ! -f sub/.gitignore ]; then
	echo "Error: sub/.gitignore file not generated" 1>&2
	exit 1
fi

gitignore_bad foo.c .gitignore
gitignore_bad bar.c .gitignore
gitignore_bad Tupfile .gitignore
gitignore_good foo.o .gitignore
gitignore_good bar.o .gitignore
gitignore_good prog .gitignore
gitignore_bad shazam.c sub/.gitignore
gitignore_bad Tupfile sub/.gitignore
gitignore_good shazam.o sub/.gitignore
gitignore_good libsub.a sub/.gitignore

rm -f Tuprules.tup
tup rm Tuprules.tup
update
if [ -f .gitignore ]; then
	echo "Error: .gitignore exists when it shouldn't" 1>&2
	exit 1
fi
if [ -f sub/.gitignore ]; then
	echo "Error: sub/.gitignore exists when it shouldn't" 1>&2
	exit 1
fi
