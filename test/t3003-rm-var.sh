#! /bin/sh -e

# Check if we can remove a variable.

. ./tup.sh
varsetall FOO=n BAR=n
tup read
tup_object_exist @ FOO
tup_object_exist @ BAR

vardict_exist FOO
vardict_exist BAR

varsetall BAR=n
tup read
tup_object_exist @ BAR
tup_object_no_exist @ FOO

vardict_exist BAR
vardict_no_exist FOO

eotup
