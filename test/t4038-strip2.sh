#! /bin/sh -e

# Same as t4037, but in a subdirectory.

. ./tup.sh

tmkdir sub
cat > sub/foo.c << HERE
int main(void)
{
	return 0;
}
HERE
cat > sub/bar.c << HERE
void bar(void) {}
HERE
cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> ar cr %o %f && strip %o |> libfoo.a
HERE
tup touch sub/foo.c sub/bar.c sub/Tupfile
update

eotup
