#! /bin/sh -e

. ./tup.sh
check_monitor_supported

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
sleep 1
touch Tupfile
tup monitor
update
stop_monitor

tup_object_exist . 'echo yo'
tup_object_no_exist . 'echo hey'

eotup
