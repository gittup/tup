lib.a: lib.o
	ar cru $@ $<

%.o: %.c
	gcc -o $@ -c $<

clean:
	rm -f lib.o lib.a
