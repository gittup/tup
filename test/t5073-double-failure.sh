#! /bin/sh -e

# Make sure we get useful error messages if we are missing dependencies and the
# command fails. The command could be failing because the missing input
# dependency was incorrect, and fixing that file may not result in a proper
# error message because it won't be updated before the failed command.
#
# Below, we forget to add libfoo.a as an input. Also, libfoo.a is missing the
# definition for bar(). The compile fails because bar() is missing. If we then
# add bar() to libfoo.a, the compilation for prog may happen before libfoo.a
# because the input dependencies were never checked. In this case we will
# continue to see the error message that bar is undefined, because libfoo.a
# is not updated in time. This is confusing because you don't know that the
# real error is that libfoo.a is not specified as an input.
. ./tup.sh

tmkdir sub
cat > sub/Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o {objs}
: {objs} |> ar crs %o %f |> libfoo.a
HERE
cat > sub/lib.c << HERE
int foo(void) {return 3;}
HERE

tup touch sub/Tupfile sub/lib.c
update

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: main.o |> gcc %f -o %o sub/libfoo.a |> prog
HERE
cat > main.c << HERE
int foo(void);
int bar(void);
int main(void)
{
	if(foo() + bar() == 5)
		return 0;
	return -1;
}
HERE
tup touch Tupfile main.c
update_fail_msg "undefined reference.*bar" "Missing input dependency"

cat > sub/lib2.c << HERE
int bar(void) {return 2;}
HERE
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
: main.o | sub/libfoo.a |> gcc %f -o %o sub/libfoo.a |> prog
HERE
tup touch sub/lib2.c Tupfile
update

eotup
