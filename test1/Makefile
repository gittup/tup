main: main.o lib.a
	gcc -o $@ $< lib.a

lib.a: lib.o
	ar cru $@ $<

%.o: %.c
	gcc -o $@ -c $<

clean:
	rm -f lib.o main.o lib.a main
