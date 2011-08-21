#! /bin/sh -e

# Make sure .gitignore generates the standard list if a .git directory is
# present.

. ./tup.sh

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
update
gitignore_good .gitignore .gitignore

eotup
