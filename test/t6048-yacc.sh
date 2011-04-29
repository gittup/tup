#! /bin/sh -e

# Mimic an old-school tool like yacc which uses the same temp file for every
# invocation.

. ./tup.sh
check_no_windows shell

cat > ok.sh << HERE
echo "\$1" > tmp.txt
mv tmp.txt \$2
HERE
chmod +x ok.sh
cat > Tupfile << HERE
: foreach *.in |> ./ok.sh %f %o |> %B.out
HERE
tup touch ok.sh Tupfile 0.in 1.in 2.in 3.in 4.in 5.in 6.in 7.in 8.in 9.in
update -j10

for i in `seq 0 9`; do
	if ! grep $i.in $i.out > /dev/null; then
		echo "Error: $i.out should contain \"$i.in\"" 2>&1
		exit 1
	fi
done

eotup
