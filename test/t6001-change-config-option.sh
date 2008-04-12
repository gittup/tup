#! /bin/sh -e

. ../tup.sh
cat > Makefile << HERE
CONFIG_FOO := 1

srcs := bar.c
ifeq (1,\$(CONFIG_FOO))
srcs += foo.c
endif

objs := \$(srcs:.c=.o)
prog := prog

# Change the directory "." to nothing - all other actual directory names get
# a / appended
d := \$(if \$(filter .,\$(TUPWD)),,\$(TUPWD)/)

\$(prog): \$(objs)
	@if [ "x$^" != "x" ]; then \
	echo "Link '\$(patsubst %,\$d%,\$^)' into '\$d\$@'"; \
	tup command "tup wrap gcc \$(patsubst %,\$d%,\$^) -o \$d\$@" && \
	\$(foreach f,\$^,tup input \$f "tup wrap gcc \$(patsubst %,\$d%,\$^) -o \$d\$@" &&) \
	tup output "tup wrap gcc \$(patsubst %,\$d%,\$^) -o \$d\$@" \$@; \
	fi
\$(objs): %.o: %.c
	@echo "Compile '\$d\$<' into '\$d\$@'"; \
	tup command "tup wrap gcc -c \$d\$< -o \$d\$@" && \
	tup input \$< "tup wrap gcc -c \$d\$< -o \$d\$@" && \
	tup output "tup wrap gcc -c \$d\$< -o \$d\$@" \$@

.PHONY: \$(prog) \$(srcs) \$(objs)
HERE

echo "int main(void) {} void bar(void) {}" > bar.c
echo "void foo(void) {}" > foo.c
tup touch foo.c bar.c Makefile
update
sym_check foo.o foo
sym_check bar.o bar main
sym_check prog foo bar main

cat Makefile | sed 's/CONFIG_FOO := 1/CONFIG_FOO := 0/' > tmpMakefile
mv tmpMakefile Makefile
tup touch Makefile
update

sym_check bar.o bar main
sym_check prog bar main ~foo
check_not_exist foo.o
