#! /bin/sh -e

# Make sure we can remove the tup.config file to clear all the variables.

. ./tup.sh
varsetall FOO=n BAR=n
tup read
tup_object_exist @ FOO
tup_object_exist @ BAR

vardict_exist FOO
vardict_exist BAR

rm tup.config
tup read

tup_object_no_exist @ BAR
tup_object_no_exist @ FOO

vardict_no_exist BAR
vardict_no_exist FOO

eotup
