BUILD := build/
srcs := $(wildcard src/*/*.c) src/wrapper/mozilla-sha1/sha1.c src/debug.c
objs := $(addprefix $(BUILD),$(srcs:.c=.o))
deps := $(objs:.o=.d)

PROGS := monitor wrapper benchmark
SHLIBS := ldpreload.so

all: $(PROGS) $(SHLIBS)

Q=@

wrapper: LDFLAGS := -lpthread
ldpreload.so: CCFLAGS := -fpic
ldpreload.so: LDFLAGS := -ldl

# Explicitly set this, since it doesn't get picked up from the ldpreload.so
# dependency
$(BUILD)src/debug.o: CCFLAGS := -fpic

wrapper: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/wrapper/*.c)) $(BUILD)src/debug.o $(BUILD)src/wrapper/mozilla-sha1/sha1.o
monitor: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/monitor/*.c)) $(BUILD)src/debug.o
benchmark: $(patsubst %.c,$(BUILD)%.o,$(wildcard src/benchmark/*.c))

$(PROGS):
	$Qecho "  LD      $@";\
	gcc -o $@ $^ $(LDFLAGS)

ldpreload.so: $(filter $(BUILD)src/ldpreload/%,$(objs)) $(BUILD)src/debug.o
	$Qecho "  LD.so   $@";\
	gcc $(CCFLAGS) $(LDFLAGS) -shared -o $@ $^

$(objs): $(BUILD)%.o: %.c Makefile
	$Qecho "  CC      $<";\
	mkdir -p $(@D);\
	gcc -MMD $(CCFLAGS) -Isrc -c $< -o $@ -W -Wall -Werror -Wbad-function-cast -Wcast-align -Wcast-qual -Wchar-subscripts -Winline -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings -fno-common

-include $(deps)
