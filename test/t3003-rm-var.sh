#! /bin/sh -e

# Check if we can remove a variable.

. ../tup.sh
tup varset CONFIG_FOO n
tup varset CONFIG_BAR n
update
tup_object_exist @ CONFIG_FOO
tup_object_exist @ CONFIG_BAR

# Hack-ish - this is effectively what gets done in tup's kconfig. It moves all
# variables to delete.
sqlite3 .tup/db 'update node set flags=flags|4 where dir=2 and type=3'

tup varset CONFIG_BAR y
update
tup_object_exist @ CONFIG_BAR
tup_object_no_exist @ CONFIG_FOO
