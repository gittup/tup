#! /bin/sh -e

# Try to use a python client.

. ./tup.sh
check_no_windows client

if ! which python > /dev/null 2>&1; then
	echo "[33mNo python found - skipping test.[0m"
	eotup
fi

cat > foo.py << HERE
import tup_client
if tup_client.config_var('FOO') is not None:
	raise Exception("No: FOO")
if tup_client.config_var('BAR') is not None:
	raise Exception("No: BAR")
if tup_client.config_var('BAZ') is not None:
	raise Exception("No: BAZ")
HERE
cat > Tupfile << HERE
: |> PYTHONPATH=../.. python -B foo.py |>
HERE
tup touch Tupfile
update

tup_object_exist @ BAZ

varsetall FOO=y BAR=hey
update_fail_msg 'Exception: No: FOO'

cat > foo.py << HERE
import tup_client
if tup_client.config_var('FOO') != "y":
	raise Exception("No: FOO")
if tup_client.config_var('BAR') != "hey":
	raise Exception("No: BAR")
if tup_client.config_var('BAZ') is not None:
	raise Exception("No: BAZ")
HERE
tup touch foo.py
update

tup_object_exist @ BAZ

cat > foo.py << HERE
import tup_client
if tup_client.config_var('FOO') != "y":
	raise Exception("No: FOO")
if tup_client.config_var('BAR') != "hey":
	raise Exception("No: BAR")
HERE
tup touch foo.py
update

tup_object_no_exist @ BAZ

eotup
