#! /bin/sh -e

# Apparently there's a parser bug! That's what I get for not using a real
# parser generator. In the second line of the Tupfile, the {objs} slurps too
# many characters, so it will look for the file "ib" instead of "lib".
#
# I don't care about the lib, so I just use ls instead of a linking command.

. ./tup.sh
cat > Tupfile << HERE
: foreach *.c |> gcc -c %f -o %o |> %B.o {objs}
: {objs} lib |> ls %f |>
HERE
tup touch Tupfile foo.c bar.c lib
update

eotup
