WRAPPER := ../../../wrapper

a.out: lex.yy.c
	$(WRAPPER) gcc $<
lex.yy.c: csv2html.l
	$(WRAPPER) lex $<
clean:
	rm -f a.out lex.yy.c
