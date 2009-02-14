#! /bin/sh -e

# Check if we can remove a variable.

. ../tup.sh
tup varset CONFIG_FOO n
tup varset CONFIG_BAR n
update
tup_object_exist @ CONFIG_FOO
tup_object_exist @ CONFIG_BAR

tup kconfig_pre_delete
tup varset CONFIG_BAR y
tup kconfig_pre_delete
update
tup_object_exist @ CONFIG_BAR
tup_object_no_exist @ CONFIG_FOO
