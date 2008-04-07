BUILD := build/
srcs := $(wildcard src/*/*.c src/tup/mozilla-sha1/*.c)
objs := $(addprefix $(BUILD),$(srcs:.c=.o))
deps := $(objs:.o=.d)

PROGS := monitor wrapper benchmark create_dep updater depgraph config tuptouch tupcmd
SHLIBS := ldpreload.so make.so

all: $(PROGS) $(SHLIBS)

Q=@

wrapper: LDFLAGS := -lpthread
ldpreload.so: CCFLAGS := -fpic
ldpreload.so: LDFLAGS := -ldl
libtup.a: CCFLAGS := -fpic
updater: LDFLAGS := -ldl
make.so: CCFLAGS := -fpic

wrapper: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/wrapper/*.c)) libtup.a
updater: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/updater/*.c)) libtup.a
monitor: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/monitor/*.c)) libtup.a
make.so: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/make/*.c)) libtup.a
ldpreload.so: $(filter $(BUILD)src/ldpreload/%,$(objs)) libtup.a
benchmark: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/benchmark/*.c))
create_dep: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/create_dep/*.c)) libtup.a
depgraph: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/depgraph/*.c)) libtup.a
config: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/config/*.c)) libtup.a
tuptouch: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/tuptouch/*.c)) libtup.a
tupcmd: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/tupcmd/*.c)) libtup.a

$(PROGS):
	$Qecho "  LD      $@";\
	gcc -o $@ $^ $(LDFLAGS)

$(SHLIBS): %.so:
	$Qecho "  LD.so   $@";\
	gcc $(CCFLAGS) $(LDFLAGS) -shared -o $@ $^

libtup.a: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/tup/*.c) $(wildcard src/tup/mozilla-sha1/*.c))
	$Qecho "  AR      $@";\
	rm -f $@ ;\
	ar cru $@ $^

$(objs): $(BUILD)%.o: %.c Makefile
	$Qecho "  CC      $<";\
	mkdir -p $(@D);\
	gcc -MMD $(CCFLAGS) -Isrc -c $< -o $@ -W -Wall -Werror -Wbad-function-cast -Wcast-align -Wcast-qual -Wchar-subscripts -Winline -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings -fno-common

-include $(deps)
