#! /bin/sh -e

# I used to do a weird thing where I'd append characters after a %f before the
# next space after each filename. So here, "cat %f;" would become "cat foo.txt;
# bar.txt;". I think this was so I could do "ld %F.o" when I didn't have the
# object files in the DAG (ie: this was a long time ago). I don't think it's
# necessary anymore and is confusing.

. ./tup.sh
check_no_windows shell
cat > Tupfile << HERE
: foo.txt bar.txt |> cat %f; echo yay |>
HERE
echo "foo" > foo.txt
echo "bar" > bar.txt
tup touch Tupfile foo.txt bar.txt
update

eotup
