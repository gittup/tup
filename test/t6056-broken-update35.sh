#! /bin/sh -e

# If a sub-process tries to go into the .tup directory, it may hit the fuse
# mountpoint and hang since we're already in fuse at that point. Subprocesses
# shouldn't be looking at .tup anyway.

. ./tup.sh

cat > Tupfile << HERE
: |> find . > %o |> files.txt
HERE
tup touch Tupfile
update

eotup
