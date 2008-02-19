WRAPPER := ../../wrapper

all: build/prog/main build/prog2/main

build/prog2/main: build/prog2/main.o build/lib/lib.a
	mkdir -p $(@D) && $(WRAPPER) gcc $(LDFLAGS) -o $@ $^

build/prog/main: build/prog/main.o build/lib/lib.a
	mkdir -p $(@D) && $(WRAPPER) gcc $(LDFLAGS) -o $@ $^

build/lib/lib.a: build/lib/lib.o
	mkdir -p $(@D) && $(WRAPPER) ar cru $@ $^

build/%.o: %.c
	mkdir -p $(@D) && $(WRAPPER) gcc $(CCFLAGS) -o $@ -c $< -I.

clean:
	rm -rf build .tup

.PHONY: all clean
