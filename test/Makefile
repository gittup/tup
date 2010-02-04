T := $(wildcard t[0-9][0-9][0-9][0-9]-*.sh)

testnum = $(firstword $(subst -, ,$1))
all: $T

$T:
	@n=$(call testnum,$@); \
	echo ;\
	echo "[36m --- Run $@ ---[0m"; \
	dir=$$PWD; \
	rm -rf tuptesttmp-$$n; \
	mkdir tuptesttmp-$$n; \
	if [ -d "$n" ]; then cp -Rdp $$n/* tuptesttmp-$$n/; fi; \
	cd tuptesttmp-$$n; \
	tup init --no-sync --force; \
	../$@;\
	rc=$$?;\
	tup stop;\
	if [ $$rc -eq 0 ]; then \
	cd $$dir; rm -rf tuptesttmp-$$n; exit 0; \
	else exit 1; fi

.PHONY: all $T
