#! /bin/sh -e

# .gitignore files can be created, but tup may lose track of them if a dependent
# directory is being parsed.

. ./tup.sh

tmkdir sub
cat > Tupfile << HERE
: sub/foo.c |> echo %f |>
: bork
HERE
cat > sub/Tupfile << HERE
.gitignore
: |> touch %o |> hey
HERE
tup touch Tupfile sub/Tupfile sub/foo.c
update_fail_msg 'Error parsing Tupfile'

cat > Tupfile << HERE
: sub/foo.c |> echo %f |>
HERE
tup touch Tupfile
update

eotup
