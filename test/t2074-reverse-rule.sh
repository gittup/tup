#! /bin/sh -e

# Try a rule where we use the output names to drive the foreach

. ./tup.sh
check_no_windows symlink
cat > Tupfile << HERE
link-y += ar
link-y += vi
: foreach \$(link-y) <| ln -s %f %o <| busybox
HERE
tup touch busybox Tupfile
update

tup_dep_exist . 'ln -s busybox ar' . ar
tup_dep_exist . 'ln -s busybox vi' . vi

eotup
