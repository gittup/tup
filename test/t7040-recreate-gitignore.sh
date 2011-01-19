#! /bin/sh -e

# Similar to t2077, only with the file monitor running.

. ./tup.sh
check_monitor_supported
tup monitor

cat > Tupfile << HERE
.gitignore
: |> echo foo > %o |> foo
HERE
update

gitignore_good foo .gitignore

rm .gitignore
update

gitignore_good foo .gitignore

eotup
