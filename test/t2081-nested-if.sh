#! /bin/sh -e

# Try a nested if.

. ./tup.sh
cat > Tupfile << HERE
ifeq (@(FOO),y)
: |> foo y |>
ifeq (@(BAR),y)
: |> foobar y |>
else
: |> foo y bar n |>
endif
else
: |> foo n |>
endif
HERE
tup touch Tupfile
varsetall FOO=y BAR=y
tup parse
tup_object_exist . 'foo y'
tup_object_exist . 'foobar y'
tup_object_no_exist . 'foo y bar n'
tup_object_no_exist . 'foo n'

varsetall FOO=y BAR=n
tup parse
tup_object_exist . 'foo y'
tup_object_no_exist . 'foobar y'
tup_object_exist . 'foo y bar n'
tup_object_no_exist . 'foo n'

varsetall FOO=n BAR=y
tup parse
tup_object_no_exist . 'foo y'
tup_object_no_exist . 'foobar y'
tup_object_no_exist . 'foo y bar n'
tup_object_exist . 'foo n'

varsetall FOO=n BAR=n
tup parse
tup_object_no_exist . 'foo y'
tup_object_no_exist . 'foobar y'
tup_object_no_exist . 'foo y bar n'
tup_object_exist . 'foo n'

eotup
