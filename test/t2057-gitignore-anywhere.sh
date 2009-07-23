#! /bin/sh -e

# Make sure .gitignore works anywhere in the Tupfile

. ../tup.sh

cat > Tupfile << HERE
.gitignore
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
HERE
echo 'int main(void) {return 0;}' > foo.c

tup touch foo.c bar.c Tupfile
update

if [ ! -f .gitignore ]; then
	echo "Error: .gitignore file not generated" 1>&2
	exit 1
fi

gitignore_bad foo.c .gitignore
gitignore_bad bar.c .gitignore
gitignore_bad Tupfile .gitignore
gitignore_good foo.o .gitignore
gitignore_good bar.o .gitignore
gitignore_good prog .gitignore
