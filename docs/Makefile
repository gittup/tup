all: $(patsubst %.dot,%.png,$(wildcard *.dot))
%.png: %.dot Makefile
	dot -Efontname="Luxi Sans" -Nfontname="Luxi Sans" -Efontsize=10 -Nfontsize=10 -Tpng $< > $@
