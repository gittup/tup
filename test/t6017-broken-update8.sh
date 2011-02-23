#! /bin/sh -e

# For some reason switching from 'gcj -C %f' to 'gcj -c %f' ends up not
# creating a link from B.java to the command for A.o, when class A uses B.
# Turns out this is because the second time around, compiling A.java reads
# from B.class instead of B.java. Then B.class is deleted (because the command
# to create it is gone, and we're creating .o files now) so the dependency is
# gone. I solved this by moving the file deletions out into its own phase.
#
# This test case merely mimics this process because when I upgraded to gcc
# 4.3.2 from 4.1.something, gcj became slow as crap. The 'ls | grep' part is
# to mimic the getdents() syscall that javac and gcj appear to use.

. ./tup.sh
check_no_windows shell

cat > Tupfile << HERE
: B.java |> cat %f > %o |> B.class
: A.java | B.class |> (if ls | grep B.class > /dev/null; then echo "Using B.class"; cat B.class; else echo "Using B.java"; cat B.java; fi; cat %f) > %o |> A.class
HERE
echo "A" > A.java
echo "B" > B.java
tup touch A.java B.java Tupfile
update
check_exist A.class B.class
echo 'B' | diff - B.class
(echo 'Using B.class'; echo 'B'; echo 'A') | diff - A.class

cat > Tupfile << HERE
: B.java |> cat %f > %o |> B.o
: A.java |> (if ls | grep B.class > /dev/null; then echo "Using B.class"; cat B.class; else echo "Using B.java"; cat B.java; fi; cat %f) > %o |> A.o
HERE
tup touch Tupfile
update
check_not_exist A.class B.class
check_exist A.o B.o
echo 'B' | diff - B.o
(echo 'Using B.java'; echo 'B'; echo 'A') | diff - A.o

eotup
