#! /bin/sh -e

# Make sure we can export environment variables.

export FOO="hey"
. ./tup.sh

cat > Tupfile << HERE
: |> sh ok.sh > %o |> out.txt
HERE
cat > ok.sh << HERE
echo "foo is \$FOO"
HERE
update
echo 'foo is ' | diff - out.txt

cat > Tupfile << HERE
export FOO
: |> sh ok.sh > %o |> out.txt
HERE
tup touch Tupfile
update
echo 'foo is hey' | diff - out.txt

export FOO="yo"
update
echo 'foo is yo' | diff - out.txt

cat > Tupfile << HERE
export FOO
: |> ^ run script > %o^ sh ok.sh > %o |> out.txt
HERE
tup touch Tupfile
update
echo 'foo is yo' | diff - out.txt

tup_dep_exist . ok.sh . '^ run script > out.txt^ sh ok.sh > out.txt'
tup_dep_no_exist $ FOO 0 .
tup_dep_exist $ FOO . '^ run script > out.txt^ sh ok.sh > out.txt'

cat > Tupfile << HERE
: |> ^ run script > %o^ sh ok.sh > %o |> out.txt
HERE
tup touch Tupfile
update
echo 'foo is ' | diff - out.txt

# Have to modify the environment variable before tup removes it.
export FOO="latest"
update
tup_object_no_exist $ FOO

eotup
