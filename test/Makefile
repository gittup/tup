T := $(wildcard t[0-9][0-9][0-9][0-9]-*.sh)
B := $(wildcard b[0-9][0-9]-*.sh)

testnum = $(firstword $(subst -, ,$1))
all: $T
bench: $B
	@echo " ----------------------------------"; \
	echo -n "[34m Summary:[0m		"; \
	t=`cat tupbenchtmps.t | awk '{x+=$$1} END{print x}'`; \
	d=`cat tupbenchtmps.d | awk '{x+=$$1} END{print x}'`; \
	echo "$${t}s $${d}k"; \
	rm -f tupbenchtmps.[td]
NUM := 100

$T:
	@n=$(call testnum,$@); \
	echo ;\
	echo "[34m --- Run $@ ---[0m"; \
	dir=$$PWD; \
	rm -rf tuptesttmp-$$n; \
	mkdir tuptesttmp-$$n; \
	if [ -d "$n" ]; then cp -Rdp $$n/* tuptesttmp-$$n/; fi; \
	cd tuptesttmp-$$n; \
	tup init; \
	if ../$@; then cd $$dir; rm -rf tuptesttmp-$$n; exit 0; else exit 1; fi

$B: clearmarks
	@n=$(call testnum,$@); \
	echo -n "[34m Bench[$(@:.sh=)]:[0m	"; \
	dir=$$PWD; \
	rm -rf tupbenchtmp-$$n; \
	mkdir tupbenchtmp-$$n; \
	cd tupbenchtmp-$$n; \
	t=`((time -p (tup init > /dev/null; ../$@ $(NUM) > /dev/null 2>&3); echo -n MARF$$?FRAM 1>&4) 2>&1 | grep ^real | awk '{print $$2}') 3>&1 4>&1 | sed 's/MARF0FRAM//'`; \
	if echo "$$t" | grep MARF > /dev/null; then echo $$t; exit 1; fi; \
	d=`du -sk .tup | awk '{print $$1}'`; \
	cd $$dir; \
	rm -rf tupbenchtmp-$$n; \
	echo $${t}s $${d}k; \
	echo $${t} >> tupbenchtmps.t; \
	echo $${d} >> tupbenchtmps.d

clearmarks: ; @echo "" > tupbenchtmps.t; echo "" > tupbenchtmps.d

ifneq (,$(filter bench,$(MAKECMDGOALS)))
.NOTPARALLEL:
endif
.PHONY: all bench clearmarks $T $B
