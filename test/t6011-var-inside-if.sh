#! /bin/sh -e

# Make sure if a variable is set inside an if statement (and used within that
# if statement), we don't care that it is unset.
. ../tup.sh
cat > Tupfile << HERE
VAR = 0
ifeq (\$(VAR),1)
CC = gcc
: foreach *.c |> \$(CC) -c %f -o %o |> %F.o
endif
HERE

tup touch foo.c Tupfile
update
