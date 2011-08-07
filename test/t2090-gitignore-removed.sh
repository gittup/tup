#! /bin/sh -e

# Make sure the .gitignore file is removed if the directive is no longer
# in a Tupfile.

. ./tup.sh

cat > Tupfile << HERE
.gitignore
: |> touch %o |> foo
HERE
tup touch Tupfile
update

gitignore_good foo .gitignore

cat > Tupfile << HERE
: |> touch %o |> foo
HERE
tup touch Tupfile
update

check_not_exist .gitignore

eotup
