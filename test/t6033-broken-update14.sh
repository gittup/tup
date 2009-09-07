#! /bin/sh -e

# While trying to generate some of the files in gcc, I ended up with a weird
# case where I generate the same file twice (here, errors.o), and in one case
# use that to create an exe that ends up creating a generated header. That
# generated header is then used as an input in the second exe. This
# demonstrates two issues:
#
# 1) Two different rules shouldn't create the same command
# 2) Circular dependencies aren't getting detected.
#
# The result instead was a fatal tup error since the graph isn't empty.

. ../tup.sh
cat > Tupfile << HERE
!cc = | \$(generated_headers) |> gcc -c %f -o %o |>
: foreach errors.c |> !cc |> errors.o
: errors.o |> gcc %f -o %o |> errors1
generated_headers = foo.h
: errors1 |> cat errors1 > /dev/null && echo '#define FOO 3' > %o |> foo.h
: foreach errors.c |> !cc |> errors.o
: errors.o |> gcc %f -o %o |> errors2
HERE
echo 'int main(void) {return 0;}' > errors.c
tup touch errors.c Tupfile
update_fail_msg "Attempted to add duplicate command ID"
