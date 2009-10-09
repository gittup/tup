#! /bin/sh -e

. ../tup.sh

# Apparently changing a Tupfile in between monitor invocations doesn't work
# properly (it doesn't get re-parsed).
tup monitor
cat > Tupfile << HERE
: |> echo hey |>
HERE
update
stop_monitor
tup_object_exist . 'echo hey'

cat > Tupfile << HERE
: |> echo yo |>
HERE
# Same excuse as in t7004
touch -t 202005080000 Tupfile
tup monitor
update
stop_monitor

tup_object_exist . 'echo yo'
tup_object_no_exist . 'echo hey'
