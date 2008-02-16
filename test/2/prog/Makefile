main: main.o ../lib/lib.a
	$(WRAPPER) gcc -o $@ $< ../lib/lib.a

%.o: %.c
	$(WRAPPER) gcc -o $@ -c $< -I..

clean:
	rm -rf main.o main *.tupd
