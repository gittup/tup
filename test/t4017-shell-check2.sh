#! /bin/sh -e

# More canonicalization issues. Create a file called 'bar.sh', then read
# './bar.sh', then unlink it. This apparently worked before when I was
# canonicalizing everything, but now it doesn't.

. ../tup.sh
cat > Tupfile << HERE
: |> ./foo.sh |>
HERE

cat > foo.sh << HERE
echo 'echo hey' > bar.sh
chmod +x bar.sh
./bar.sh
rm bar.sh
HERE
chmod +x foo.sh
tup touch Tupfile foo.sh
update
