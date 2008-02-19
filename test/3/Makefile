WRAPPER := ../../wrapper

all: prog/main prog2/main

prog2/main: prog2/main.o lib/lib.a
	$(WRAPPER) gcc $(LDFLAGS) -o $@ $^

prog/main: prog/main.o lib/lib.a
	$(WRAPPER) gcc $(LDFLAGS) -o $@ $^

lib/lib.a: lib/lib.o
	$(WRAPPER) ar cru $@ $^

%.o: %.c
	$(WRAPPER) gcc $(CCFLAGS) -o $@ -c $< -I.

clean:
	rm -rf .tup lib/lib.o lib/lib.a prog/main prog/main.o prog2/main prog2/main.o

.PHONY: all clean
