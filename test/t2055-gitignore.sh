#! /bin/sh -e

# Try to use the .gitignore directive

function bad()
{
	if grep $1 $2 > /dev/null; then
		echo "Error: $1 found in .gitignore" 1>&2
		exit 1
	fi
}

function good()
{
	if grep $1 $2 > /dev/null; then
		:
	else
		echo "Error: $1 not found in .gitignore" 1>&2
		exit 1
	fi
}

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
: *.o |> rm -f %o; ar cr %o %f |> libsub.a
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

bad foo.c .gitignore
bad bar.c .gitignore
bad Tupfile .gitignore
good foo.o .gitignore
good bar.o .gitignore
good prog .gitignore
bad shazam.c sub/.gitignore
bad Tupfile sub/.gitignore
good shazam.o sub/.gitignore
good libsub.a sub/.gitignore

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
