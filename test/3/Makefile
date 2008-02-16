WRAPPER := /home/mjs/tup/wrapper

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
	rm -rf lib/lib.o lib/lib.a lib/*.tupd prog/main prog/main.o prog/*.tupd prog2/main prog2/main.o prog2/*.tupd

.PHONY: all clean
