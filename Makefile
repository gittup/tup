BUILD := build/
srcs := $(wildcard src/*/*.c src/tup/tup/*.c)
objs := $(addprefix $(BUILD),$(srcs:.c=.o))
deps := $(objs:.o=.d)

all: tup ldpreload.so

Q=@

tup: LDFLAGS := -lsqlite3
ldpreload.so: CCFLAGS := -fpic
ldpreload.so: LDFLAGS := -ldl -lsqlite3

tup: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/tup/tup/*.c)) libtup.a
ldpreload.so: $(filter $(BUILD)src/ldpreload/%,$(objs))

# The git status command seems to get rid of files that may have been touched
# but aren't modified. Not really sure why that is, or if there's a better way
# to do it.
tup:
	$Qgit status > /dev/null 2>&1;\
	if test -z "$$(git diff-index --name-only HEAD)"; then mod=""; else mod="-mod"; fi ;\
	version=`git describe`; \
	echo "  VERSION $${version}$${mod}"; \
	echo "const char *tup_version(void) {return \"$${version}$${mod}\";}" | gcc -x c -c - -o $(BUILD)tup_version.o; \
	echo "  LD      $@";\
	gcc -o $@ $^ $(BUILD)tup_version.o $(LDFLAGS)

ldpreload.so:
	$Qecho "  LD.so   $@";\
	gcc $(CCFLAGS) $(LDFLAGS) -shared -o $@ $^

libtup.a: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/tup/*.c))
	$Qecho "  AR      $@";\
	rm -f $@ ;\
	ar cru $@ $^

$(objs): $(BUILD)%.o: %.c Makefile
	$Qecho "  CC      $<";\
	mkdir -p $(@D);\
	gcc -Os -MMD $(CCFLAGS) -Isrc -c $< -o $@ -W -Wall -Werror -Wbad-function-cast -Wcast-align -Wcast-qual -Wchar-subscripts -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings -fno-common

-include $(deps)

clean:
	rm -f tup ldpreload.so; rm -rf build
