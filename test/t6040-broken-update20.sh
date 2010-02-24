#! /bin/sh -e

# Ran into a rather annoying problem when changing tup.config and also
# removing something that linked to a variable. The problem is in the first
# phase, the scan finds the missing file and removes it and all its links
# from the database. The other end of the removed links are checked for
# possible ghosts, which adds their tup_entrys to the entry tree. If an
# entry is a variable, that variable will fail to get added properly if
# tup.config was also changed, resulting in a database error. Also due to
# my poor error handling logic, the error was partially committed (I think),
# making it difficult to reproduce.
#
# This test reproduces the issue by making a directory dependent on @(ARCH),
# and then removing the dir and modifying tup.config in the same step.

. ./tup.sh

tmkdir foo
cat > foo/Tupfile <<HERE
files-@(ARCH) = foo.c
HERE
varsetall ARCH=y
tup touch foo/Tupfile
update

rm -rf foo
# Don't 'tup rm' foo so the scan will pick up the removal
tup touch tup.config
update

eotup
