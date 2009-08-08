#! /bin/sh -e

# Make sure .gitignore generates the standard list if a .git directory is
# present.

. ../tup.sh

cat > Tupfile << HERE
.gitignore
HERE

mkdir .git
tup touch Tupfile
update

if [ ! -f .gitignore ]; then
	echo "Error: .gitignore file not generated" 1>&2
	exit 1
fi

gitignore_good .tup .gitignore
gitignore_good .*.swp .gitignore
gitignore_good .gitignore .gitignore
