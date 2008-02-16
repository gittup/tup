all: $(patsubst %.dot,%.png,$(wildcard *.dot))
%.png: %.dot Makefile
	dot -Tpng $< > $@
