#! /bin/sh -e

# Slightly more complex ghost dir - see if we have a ghost node (ghost) with a
# dir pointing to another ghost node (secret). Then we make secret a file, and
# then we remove secret and make it a directory, and then create ghost in it.
# Make sure that we don't lose ghost dependencies and that all works somehow.

. ./tup.sh
# Windows adds a dependency from 'secret' to the command - both are valid
# options.
check_no_windows
cat > ok.sh << HERE
cat secret/ghost 2>/dev/null || echo nofile
HERE
cat > Tupfile << HERE
: |> ./ok.sh > %o |> output.txt
HERE
chmod +x ok.sh
tup touch ok.sh Tupfile
update
echo nofile | diff output.txt -
tup_dep_exist . secret . './ok.sh > output.txt'

# Create 'secret' as a file - this will cause the command to run
echo 'foo' > secret
tup touch secret
update --no-scan
tup_object_exist . secret
tup_dep_exist . secret . './ok.sh > output.txt'

# Delete the file
rm -f secret
tup rm secret
update --no-scan
tup_dep_exist . secret . './ok.sh > output.txt'

# Once the dir exists we should get a dependency on 'ghost' too
tmkdir secret
update --no-scan
tup_dep_exist . secret . './ok.sh > output.txt'
tup_dep_exist secret ghost . './ok.sh > output.txt'

# Now we finally re-create ghost. The command should execute at this point.
echo 'alive' > secret/ghost
tup touch secret/ghost
update
echo alive | diff output.txt -

eotup
