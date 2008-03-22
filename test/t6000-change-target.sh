#! /bin/sh -e

. ../tup.sh

tup config build_so make.so
cat > Makefile << HERE
ifeq (1,\$(TUP_CREATE))
\$(warning create \$(MAKECMDGOALS))
\$(filter %.c,\$(MAKECMDGOALS)): ; tup link \$@ \$(@:.c=.o)
\$(filter %.o,\$(MAKECMDGOALS)): ; tup link \$@ \$(@D)/prog
else
\$(warning update \$(MAKECMDGOALS))
\$(filter %.c,\$(MAKECMDGOALS)): ;
\$(filter %.o,\$(MAKECMDGOALS)): ; tup wrap gcc -c \$(@:.o=.c) -o \$@
\$(filter %prog,\$(MAKECMDGOALS)): ; tup wrap gcc -o \$@ \$(@D)/*.o
endif
.PHONY: \$(MAKECMDGOALS)
HERE

echo "int main(void) {} void foo(void) {}" > foo.c
tup touch foo.c Makefile
update
sym_check foo.o foo
sym_check prog foo

cat Makefile | sed 's/prog/newprog/' > tmpMakefile
mv tmpMakefile Makefile
tup touch Makefile
update

sym_check newprog foo
check_not_exist prog
tup_object_no_exist prog
