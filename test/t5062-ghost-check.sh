#! /bin/sh -e

# Quick test to make sure that ghost_check won't remove real nodes. Since
# ghost_check is only for debugging, it doesn't get used very often. In theory
# I should put an unused ghost in there to make sure it gets removed, but I
# would need to fiddle with the db directly, or add more debugging
# functionality to tup.

. ./tup.sh
tup touch foo
tup_object_exist . foo

tup ghost_check
tup_object_exist . foo

eotup
