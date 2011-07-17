#! /bin/sh -e

# Make sure update --no-scan works when changing @-variables.

. ./tup.sh

varsetall FOO=y
update --no-scan

eotup
