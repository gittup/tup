all: $(patsubst %.dot,%.png,$(wildcard *.dot))
%.png: %.dot Makefile
	export DOTFONTPATH=/usr/share/fonts/ttf-bitstream-vera;\
	/home/mjs/ctmp/usr/local/bin/dot -Efontname="Luxi Sans" -Nfontname="Luxi Sans" -Efontsize=10 -Nfontsize=10 -Tpng $< > $@
