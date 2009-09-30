#! /bin/sh -e

# If a generated file hasn't been created yet, doing a scan shouldn't cause the
# directory to get put into the create list.
. ../tup.sh

check_empty_create()
{
	if tup create_flags_exists; then
		:
	else
		echo "*** Nodes shouldn't have create flags set" 1>&2
		exit 1
	fi
}

cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o
HERE
tup touch foo.c
tup parse
check_empty_create

tup scan
check_empty_create
