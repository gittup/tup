#! /bin/sh -e

# We have a ghost Tuprules.tup node, and then move a file over it. The
# necessary directories should be re-parsed.
. ./tup.sh
check_monitor_supported
tup monitor
mkdir a
mkdir a/a2
cat > a/a2/Tupfile << HERE
include_rules
: |> echo \$(VAR) |>
HERE
echo 'VAR=3' > Tuprules.tup
echo 'VAR=4' > a/ok.txt
update

tup_dep_exist a Tuprules.tup a a2
tup_object_exist a/a2 'echo 3'

mv a/ok.txt a/Tuprules.tup
update

tup_dep_exist a Tuprules.tup a a2
tup_object_exist a/a2 'echo 4'

eotup
