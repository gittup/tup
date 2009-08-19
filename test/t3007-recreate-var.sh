#! /bin/sh -e

# Changing a variable seems broken (it's still marked delete, so Tupfiles don't
# parse)

. ../tup.sh
cat > Tupfile << HERE
var = @(CONFIG_FOO)
HERE
tup touch Tupfile
tup varsetall CONFIG_FOO=n
update
tup_object_exist @ CONFIG_FOO

tup varsetall CONFIG_FOO=y
update
tup_object_exist @ CONFIG_FOO
