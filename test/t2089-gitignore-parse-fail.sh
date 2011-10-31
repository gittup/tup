#! /bin/sh -e

# Make sure the .gitignore file isn't removed if a Tupfile fails to parse.

. ./tup.sh

cat > Tupfile << HERE
.gitignore
: |> touch %o |> foo
HERE
tup touch Tupfile
update

gitignore_good foo .gitignore

cat > Tupfile << HERE
.gitignore
: |> touch %o |> foo
borkbork
HERE
tup touch Tupfile
update_fail_msg "Syntax error parsing Tupfile line 3"

gitignore_good foo .gitignore

eotup
