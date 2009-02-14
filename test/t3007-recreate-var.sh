#! /bin/sh -e

# Changing a variable seems broken (it's still marked delete, so Tupfiles don't
# parse)

. ../tup.sh
cat > Tupfile << HERE
var = @CONFIG_FOO@
HERE
tup touch Tupfile
tup varset CONFIG_FOO n
update
tup_object_exist @ CONFIG_FOO

tup kconfig_pre_delete
tup varset CONFIG_FOO y
tup kconfig_post_delete
update
tup_object_exist @ CONFIG_FOO
