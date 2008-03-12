T := $(wildcard t[0-9][0-9][0-9][0-9]-*.sh)

num = $(firstword $(subst -, ,$1))
all: $(T)

$(T):
	@n=$(call num,$@); \
	echo ;\
	echo " *** Run $@ ***"; \
	dir=$$PWD; \
	rm -rf tuptesttmp-$$n; \
	mkdir tuptesttmp-$$n; \
	if [ -d "$n" ]; then cp -Rdp $$n/* tuptesttmp-$$n/; fi; \
	cd tuptesttmp-$$n; \
	tup init; \
	if ../$@; then cd $$dir; rm -rf tuptesttmp-$$n; exit 0; else exit 1; fi

.PHONY: all $(T)
