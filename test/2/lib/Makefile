lib.a: lib.o
	$(WRAPPER) ar cru $@ $<

%.o: %.c
	$(WRAPPER) gcc -o $@ -c $<

clean:
	rm -rf lib.o lib.a .tup
