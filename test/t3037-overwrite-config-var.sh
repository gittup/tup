#! /bin/sh -e

# Make sure we can't overwrite a CONFIG_ variable, since that equates to an @-variable.

. ./tup.sh
cat > Tupfile << HERE
CONFIG_FOO = y
ifeq (\$(CONFIG_FOO),y)
: |> echo blah |> bar
endif
HERE
parse_fail_msg "Unable to override setting of variable 'CONFIG_FOO'"

eotup
