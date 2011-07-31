#! /bin/sh -e

# Execute a run-script from an included file.
. ./tup.sh

tmkdir sub
cat > sub/gen.sh << HERE
#! /bin/sh
for i in foo bar; do
	echo ": |> touch %o |> \$i"
done
HERE
chmod +x sub/gen.sh

cat > sub/inc.tup << HERE
run ./gen.sh
HERE
cat > Tupfile << HERE
include sub/inc.tup
HERE
tup touch Tupfile sub/inc.tup sub/gen.sh
update

check_exist foo bar

eotup
