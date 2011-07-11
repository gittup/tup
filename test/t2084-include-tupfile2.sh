#! /bin/sh -e

# Include multiple subdirectories of Tupfiles

. ./tup.sh
tmkdir sub1
tmkdir sub1/sub2
tmkdir sub1/sub2/sub3

echo 'include sub1/1.tup' > Tupfile
echo 'include sub2/2.tup' > sub1/1.tup
echo 'include sub3/3.tup' > sub1/sub2/2.tup
echo 'cflags += -DFOO' > sub1/sub2/sub3/3.tup
tup touch Tupfile sub1/1.tup sub1/sub2/2.tup sub1/sub2/sub3/3.tup
update

eotup
