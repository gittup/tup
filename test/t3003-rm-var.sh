#! /bin/sh -e

# Check if we can remove a variable.

. ../tup.sh
tup varsetall CONFIG_FOO=n CONFIG_BAR=n
tup_object_exist @ CONFIG_FOO
tup_object_exist @ CONFIG_BAR

vardict_exist CONFIG_FOO
vardict_exist CONFIG_BAR

tup varsetall CONFIG_BAR=n
tup_object_exist @ CONFIG_BAR
tup_object_no_exist @ CONFIG_FOO

vardict_exist CONFIG_BAR
vardict_no_exist CONFIG_FOO
