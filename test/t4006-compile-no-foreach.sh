#! /bin/sh -e
# When we explicitly list the input files (without foreach), the behavior in
# the parser is a bit different. Specifically, the output file isn't put into
# the database until later in the parsing, after the rule that needs that
# output has done a select on it. This addresses a specific bug discovered
# while tupifying sysvinit.

. ./tup.sh
cat > Tupfile << HERE
: foo.c |> gcc -c %f -o %o|> %B.o
: foo.o |> gcc %f -o prog |> prog
HERE

echo "int main(void) {}" > foo.c
tup touch foo.c Tupfile
update
sym_check foo.o main
tup_object_exist . foo.o prog

# Run a second time, since in theory this time foo.o is in the database, but
# will be moved to DELETE before the Tupfile is re-parsed. So, it's slightly
# different in this case.
tup touch foo.c Tupfile
update
sym_check foo.o main
tup_object_exist . foo.o prog

eotup
