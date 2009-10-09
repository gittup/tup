#! /bin/sh -e

# Make sure a var/sed command doesn't get re-invoked when the monitor is
# stopped and restarted.

. ../tup.sh
tup monitor

cat > Tupfile << HERE
, foo.txt |> out.txt
: out.txt |> cat %f > %o |> new.txt
HERE
echo "hey @FOO@ yo" > foo.txt
varsetall FOO=sup
update
tup_object_exist . foo.txt out.txt new.txt
(echo "hey sup yo") | diff out.txt -
(echo "hey sup yo") | diff new.txt -

echo "a @FOO@ b" > foo.txt
update
(echo "a sup b") | diff out.txt -
(echo "a sup b") | diff new.txt -

stop_monitor
tup monitor
stop_monitor
check_empty_tupdirs
