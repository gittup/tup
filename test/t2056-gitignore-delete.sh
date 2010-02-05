#! /bin/sh -e

# .gitignore with deleted files

. ./tup.sh
cat > Tuprules.tup << HERE
.gitignore
HERE

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> gcc %f -o %o |> prog
include_rules
HERE
echo 'int main(void) {return 0;}' > foo.c

tup touch foo.c bar.c Tupfile Tuprules.tup
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

rm -f bar.c
tup rm bar.c
update

gitignore_bad foo.c .gitignore
gitignore_bad bar.c .gitignore
gitignore_bad Tupfile .gitignore
gitignore_good foo.o .gitignore
gitignore_bad bar.o .gitignore
gitignore_good prog .gitignore

eotup
