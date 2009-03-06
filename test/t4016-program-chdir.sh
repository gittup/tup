#! /bin/sh -e

# Wrapping chdir might be easy - just have to get the new path to the server,
# and possibly append it to the existing path and canonicalize it. But what
# about fchdir? In any case, if I find a real tool that does a chdir before
# writing files, I may implement this.
echo "[33mSkip t4016 - haven't found any real tools that use chdir internally.[0m"
exit 0

# Let's see if the wrapper thing will work if we chdir to a new directory, and
# then touch a file back in the original directory. Obviously it doesn't make
# sense to touch a file in the new directory since that would be illegal.

. ../tup.sh

mkdir tmp
cat > ok.sh << HERE
#! /bin/sh
cd tmp
cat bar > ../foo
HERE
chmod +x ok.sh

cat > Tupfile << HERE
: tmp/bar |> ./ok.sh |> foo
HERE

echo "yo" > tmp/bar

tup touch tmp/bar Tupfile ok.sh
update
