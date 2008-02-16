main: main.o ../lib/lib.a
	gcc -o $@ $< ../lib/lib.a

%.o: %.c
	gcc -o $@ -c $< -I..

clean:
	rm -f main.o main
