#! /bin/sh -e

# Make sure when changing an existing command, the actual new command gets
# executed (not the old one). This became a problem when I introduced caching,
# since I forgot to update the cache when chaging the node name. This was not
# seen in any other tests because the command only gets cached early enough if
# there is a dependency from a directory to a command (as is the case with
# cpio - most normal commands don't have this).

. ./tup.sh

grep_yes()
{
	if grep "$1" output; then
		:
	else
		echo "Error: Expected file $1 in cpio archive" 1>&2
		exit 1
	fi
}

grep_no()
{
	if grep "$1" output; then
		echo "Error: File $1 shouldn't be in cpio archive" 1>&2
		exit 1
	else
		:
	fi
}

tmkdir sub
cat > sub/Tupfile << HERE
files-y = foo.c
files-@(BAR) += bar.c
: foreach \$(files-y) |> gcc -c %f -o %o |> %B.o
HERE

cat > Tupfile << HERE
: sub sub/*.o |> (for i in %f; do echo \$i; done) > %o |> output
HERE
echo "int main(void) {return 0;}" > sub/foo.c
echo "void bar(void) {}" > sub/bar.c
tup touch sub/foo.c sub/bar.c sub/Tupfile Tupfile
varsetall BAR=y
update

grep_yes '^sub$'
grep_yes '^sub/foo.o$'
grep_yes '^sub/bar.o$'

varsetall BAR=n
update

grep_yes '^sub$'
grep_yes '^sub/foo.o$'
grep_no '^sub/bar.o$'

eotup
