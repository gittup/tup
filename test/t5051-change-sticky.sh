#! /bin/sh -e

# Some checks to make sure sticky link changes will only cause a command to be
# re-executed when necessary (specifically, when a sticky link is removed that
# is also a normal link).
. ../tup.sh
tmkdir headers
cat > headers/Tupfile << HERE
: |> echo '#define FOO 3' > %o |> foo.h
HERE
cat > Tupfile << HERE
: foreach *.c | headers/*.h |> gcc -c %f -o %o |> %B.o
HERE
echo '#include "headers/foo.h"' > foo.c
tup touch foo.c Tupfile headers/Tupfile
update

check_exist foo.o
tup_dep_exist headers foo.h . 'gcc -c foo.c -o foo.o'
tup_dep_no_exist headers bar.h . 'gcc -c foo.c -o foo.o'

# Secretly remove foo.o
rm foo.o

# Adding a new sticky link shouldn't cause foo.o to be re-created.
cat > headers/Tupfile << HERE
: |> echo '#define FOO 3' > %o |> foo.h
: |> echo '#define BAR 3' > %o |> bar.h
HERE
tup touch headers/Tupfile
update

check_not_exist foo.o
tup_dep_exist headers foo.h . 'gcc -c foo.c -o foo.o'
tup_dep_exist headers bar.h . 'gcc -c foo.c -o foo.o'

# Removing an unused sticky link shouldn't cause foo.o to be re-created.
cat > headers/Tupfile << HERE
: |> echo '#define FOO 3' > %o |> foo.h
HERE
tup touch headers/Tupfile
update

check_not_exist foo.o
tup_dep_exist headers foo.h . 'gcc -c foo.c -o foo.o'
tup_dep_no_exist headers bar.h . 'gcc -c foo.c -o foo.o'

# Only removing a sticky link that is used should try to re-compile (and fail)
cat > headers/Tupfile << HERE
HERE
tup touch headers/Tupfile
update_fail_msg "headers/foo.h: No such file or directory"

# Fix the C file and re-build
echo '' > foo.c
tup touch foo.c
update
tup_dep_no_exist headers foo.h . 'gcc -c foo.c -o foo.o'
tup_dep_no_exist headers bar.h . 'gcc -c foo.c -o foo.o'
