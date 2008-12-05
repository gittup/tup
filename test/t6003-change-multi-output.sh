#! /bin/sh -e

echo "[33mSkip t6003 - not sure how this should work"
exit 0

. ../tup.sh
cat > Makefile << HERE
# Change the directory "." to nothing - all other actual directory names get
# a / appended
d := \$(if \$(filter .,\$(TUPWD)),,\$(TUPWD)/)

all:
	@echo "Run a command to generate multiple files"
	tup link "sh ok.sh"

.PHONY: all
HERE

cat > ok.sh << HERE
touch a
touch b
HERE

tup touch ok.sh Makefile
update
check_exist a b
check_not_exist c

cat > ok.sh << HERE
touch a
touch c
HERE

touch touch ok.sh
update

check_exist a c
check_not_exist b
