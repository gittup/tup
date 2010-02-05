#! /bin/sh -e

# Like t5013, but now with execv (at least gcc uses execv).

. ./tup.sh
cat > Tupfile << HERE
: foreach exec_test.c exe.c |> gcc %f -o %o |> %B
: exec_test exe |> ./exec_test && touch %o |> test_passed
HERE
cat > exec_test.c << HERE
#include <unistd.h>

int main(void)
{
	char * const args[] = {"exe", NULL};
	execv("./exe", args);
	return 1;
}
HERE
cat > exe.c << HERE
int main(void) {return 0;}
HERE
tup touch Tupfile exec_test.c exe.c
update

check_updates exe.c test_passed

eotup
