#! /bin/sh -e

. ../tup.sh
tup monitor
tup config autoupdate 1
cat > Tupfile << HERE
: foreach *.txt |> (echo '<html>'; cat %f; echo '</html>') > %o |> %B.html
HERE

echo "This is the index" > index.txt
echo "Another page" > page.txt
tup flush

check_exist index.html page.html
cat << HERE | diff index.html -
<html>
This is the index
</html>
HERE

# Change a file and see that it gets updated
echo "Updated index" > index.txt
tup flush
cat << HERE | diff index.html -
<html>
Updated index
</html>
HERE

# Add a new file and see it gets updated
echo "New file" > new.txt
tup flush
cat << HERE | diff new.html -
<html>
New file
</html>
HERE

# Remove a file and see that its dependent file is removed
rm page.txt
tup flush
check_not_exist page.html
