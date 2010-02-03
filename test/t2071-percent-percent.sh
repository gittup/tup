#! /bin/sh -e

# Make %% end up doing a single % in the command string

. ../tup.sh
cat > Tupfile << HERE
: |> printf "hey %%i yo\n" 3  |>
HERE
tup touch Tupfile
if tup upd | grep 'hey 3 yo' > /dev/null; then
	:
else
	echo "Error: %% not properly expanded" 1>&2
	exit 1
fi
