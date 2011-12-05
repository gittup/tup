#! /bin/sh -e

# Try to use a python client in a run script and use @-variables

. ./tup.sh
check_no_windows client

if ! which python > /dev/null 2>&1; then
	echo "[33mNo python found - skipping test.[0m"
	eotup
fi

cat > foo.py << HERE
import tup_client
var = tup_client.config_var('FOO')
if var is None:
	print ": |> echo None |>"
else:
	print ": |> echo foo", var, "|>"
HERE
cat > Tupfile << HERE
run PYTHONPATH=../.. python -B foo.py
HERE
tup touch Tupfile foo.py
update

tup_object_exist . 'echo None'

varsetall FOO=y
update
tup_object_exist . 'echo foo y'

varsetall FOO=hey
update
tup_object_exist . 'echo foo hey'

eotup
