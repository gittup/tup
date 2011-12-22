#! /bin/sh -e

# Exporting a variable that does not exist should also not exist for the
# sub-process.

. ./tup.sh

cat > foo.c << HERE
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	char *val;
	val = getenv("TUP_TEST_FOO");
	if(val == NULL)
		printf("not found\n");
	else
		printf("Got value: %s\n", val);
	return 0;
}
HERE
cat > Tupfile << HERE
: foo.c |> gcc %f -o %o |> foo
export TUP_TEST_FOO
: foo |> ./%f > %o |> output.txt
HERE
update

echo "not found" | diff - output.txt

export TUP_TEST_FOO="test"
update
echo "Got value: test" | diff - output.txt

unset TUP_TEST_FOO
update
echo "not found" | diff - output.txt

eotup
