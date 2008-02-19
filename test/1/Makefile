WRAPPER := ../../wrapper
main: main.o lib.a
	$(WRAPPER) gcc -o $@ $< lib.a

lib.a: lib.o
	$(WRAPPER) ar cru $@ $<

%.o: %.c
	$(WRAPPER) gcc -o $@ -c $<

clean:
	rm -rf lib.o main.o lib.a main .tup
