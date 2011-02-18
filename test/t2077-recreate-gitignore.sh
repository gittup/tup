#! /bin/sh -e

# If a .gitignore file gets removed, it should cause the relevant Tupfile to
# be re-parsed so it gets created again.

. ./tup.sh

cat > Tupfile << HERE
.gitignore
: |> echo foo > %o |> foo
HERE

tup touch Tupfile
update

gitignore_good foo .gitignore

rm .gitignore
update

gitignore_good foo .gitignore

eotup
