#! /bin/sh -e

# Tup has a limit of 8 nested if statements before borking.

. ./tup.sh

cat > Tupfile << HERE
ifeq (a,1)
ifeq (a,2)
ifeq (a,3)
ifeq (a,4)
ifeq (a,5)
ifeq (a,6)
ifeq (a,7)
ifeq (a,8)
endif
endif
endif
endif
endif
endif
endif
endif
HERE
tup touch Tupfile
tup parse

cat > Tupfile << HERE
ifeq (a,1)
ifeq (a,2)
ifeq (a,3)
ifeq (a,4)
ifeq (a,5)
ifeq (a,6)
ifeq (a,7)
ifeq (a,8)
ifeq (a,9)
endif
endif
endif
endif
endif
endif
endif
endif
endif
HERE
tup touch Tupfile
parse_fail_msg "too many nested if statements"

eotup
