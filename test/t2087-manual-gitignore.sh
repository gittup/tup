#! /bin/sh -e

# Make sure a manual .gitignore file doesn't get erased by the parser.

. ./tup.sh

# First create a manual .gitignore with a Tupfile
cat > Tupfile << HERE
: |> echo foo > %o |> foo
HERE

cat > .gitignore << HERE
foo
manual
HERE

tup touch Tupfile .gitignore
tup parse

gitignore_good foo .gitignore
gitignore_good manual .gitignore

# Now add the .gitignore directive and make sure we get an error
# message so we aren't overwriting our manually created file.
cat > Tupfile << HERE
.gitignore
: |> echo foo > %o |> foo
HERE
tup touch Tupfile
parse_fail_msg "Unable to create the .gitignore file"

gitignore_good foo .gitignore
gitignore_good manual .gitignore

# Finally, remove the .gitignore file and see that we can parse and
# generate the .gitignore automatically.
rm .gitignore
tup parse

gitignore_good foo .gitignore
gitignore_bad manual .gitignore

eotup
