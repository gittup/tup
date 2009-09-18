#! /bin/sh -e

# Make sure a ghost variable and a regular variable don't compare as equal if
# the values are equal (ie: both blank).

. ../tup.sh
cat > Tupfile << HERE
file-y = foo.c
file-@(BAR) += bar.c
: foreach \$(file-y) |> cat %f > %o |> %F.o
HERE
echo hey > foo.c
echo yo > bar.c
tup touch foo.c bar.c Tupfile
update
tup_object_exist . "cat foo.c > foo.o"
tup_object_no_exist . "cat bar.c > bar.o"

varsetall BAR=
tup read
if tup flags_exists 2> /dev/null; then
	echo "*** BAR variable doesn't have flags set" 1>&2
	exit 1
fi
