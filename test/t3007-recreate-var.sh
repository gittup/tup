#! /bin/sh -e

# Changing a variable seems broken (it's still marked delete, so Tupfiles don't
# parse)

. ./tup.sh
cat > Tupfile << HERE
var = @(FOO)
HERE
tup touch Tupfile
varsetall FOO=n
update
tup_object_exist @ FOO

varsetall FOO=y
update
tup_object_exist @ FOO

eotup
