#! /bin/sh -e

# Tup should parse correctly if the last newline is missing.

. ./tup.sh

cat > ok.c << HERE
#include <stdio.h>

int main(void)
{
	printf(": foreach *.c |> gcc -c %%f -o %%o |> %%B.o");
	return 0;
}
HERE
gcc ok.c -o ok
./ok > Tupfile

tup touch Tupfile ok.c ok
parse_fail_msg "Missing newline character"

cat > ok.c << HERE
#include <stdio.h>

int main(void)
{
	/* The six backslashes here becomes 3 in the C program, 2 of which
	 * become a backslash in the Tupfile, and 1 of which is used with
	 * the newline.
	 */
	printf(": foreach *.c |> \\\\\\ngcc -c %%f -o %%o |> %%B.o");
	return 0;
}
HERE
gcc ok.c -o ok
./ok > Tupfile

tup touch Tupfile
parse_fail_msg "Missing newline character"

eotup
