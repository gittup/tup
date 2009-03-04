#! /bin/sh -e

# For some reason switching from 'gcj -C %f' to 'gcj -c %f' ends up not
# creating a link from B.java to the command for A.o, when class A uses B.
# Turns out this is because the second time around, compiling A.java reads
# from B.class instead of B.java. Then B.class is deleted (because the command
# to create it is gone, and we're creating .o files now) so the dependency is
# gone. I solved this by moving the file deletions out into its own phase.
if ! which gcj; then
        echo "[33mSkip t6017 - gcj not found[0m" 1>&2
        exit 0
fi

# Apparently if I have some crap in the classpath then it can't find anything
# in the local directory. Seems to be an issue with javac too.
export CLASSPATH=""

. ../tup.sh
cat > Tupfile << HERE
: B.java |> gcj -C %f |> %B.class
: A.java | B.class |> gcj -C %f |> %B.class
HERE

cat > A.java << HERE
class A
{  
        public static void main(String args[])
        {
                System.out.println("Hello World!" + B.hey());
        }
}
HERE

cat > B.java << HERE
class B
{  
        public static String hey()
        {
                return "Yo";
        }
}
HERE

tup touch A.java B.java Tupfile
update
check_exist A.class B.class

cat > Tupfile << HERE
: foreach *.java |> gcj -c %f |> %B.o
HERE

tup touch Tupfile
update
check_not_exist A.class B.class
check_exist A.o B.o
tup_dep_exist . A.java . "gcj -c A.java"
tup_dep_exist . B.java . "gcj -c B.java"
tup_dep_exist . B.java . "gcj -c A.java"
