WRAPPER := /home/mjs/tup/wrapper

all: a.out

a.out: lex.yy.c y.tab.c
	$(WRAPPER) gcc lex.yy.c y.tab.c
lex.yy.c: scanner.l Makefile
	$(WRAPPER) lex scanner.l
y.tab.c: parser.y
	$(WRAPPER) yacc -vd parser.y
clean:
	rm -rf a.out lex.yy.c y.tab.c y.output y.tab.h .tup
