#! /bin/sh -e

# Try the strip command on an archive.

. ./tup.sh

cat > foo.c << HERE
int main(void)
{
	return 0;
}
HERE
cat > bar.c << HERE
void bar(void) {}
HERE
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: *.o |> ar cr %o %f && strip %o |> libfoo.a
HERE
tup touch foo.c bar.c Tupfile
update

eotup
