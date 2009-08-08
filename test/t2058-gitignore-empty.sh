#! /bin/sh -e

# Make sure .gitignore generates the standard list if a .git directory is
# present.

. ../tup.sh

cat > Tupfile << HERE
.gitignore
HERE

tup touch Tupfile
update

if [ ! -f .gitignore ]; then
	echo "Error: .gitignore file not generated" 1>&2
	exit 1
fi

gitignore_good .tup .gitignore

mkdir .git
# TODO: If a .git directory is created, perhaps the Tupfile should be
# re-parsed.
tup touch Tupfile
update
gitignore_good .*.swp .gitignore
gitignore_good .gitignore .gitignore
