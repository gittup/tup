BUILD := build/
srcs := $(wildcard src/*/*.c src/tup/tup/*.c)
objs := $(addprefix $(BUILD),$(srcs:.c=.o))
deps := $(objs:.o=.d)

PROGS := tup
SHLIBS := ldpreload.so make.so

all: $(PROGS) $(SHLIBS)

Q=@

tup: LDFLAGS := -lsqlite3 -ldl
ldpreload.so: CCFLAGS := -fpic
ldpreload.so: LDFLAGS := -ldl -lsqlite3
libtup.a: CCFLAGS := -fpic
make.so: CCFLAGS := -fpic

tup: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/tup/tup/*.c)) libtup.a
make.so: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/make/*.c)) libtup.a
ldpreload.so: $(filter $(BUILD)src/ldpreload/%,$(objs)) libtup.a

$(PROGS):
	$Qecho "  LD      $@";\
	gcc -o $@ $^ $(LDFLAGS)

$(SHLIBS): %.so:
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
