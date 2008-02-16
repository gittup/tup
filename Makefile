srcs := $(wildcard src/*/*.c)
objs := $(srcs:.c=.o)
deps := $(srcs:.c=.d)

PROGS := monitor wrapper
SHLIBS := ldpreload.so

all: $(PROGS) $(SHLIBS)

Q=@

wrapper: LDFLAGS := -lpthread
ldpreload.so: CCFLAGS := -fpic
ldpreload.so: LDFLAGS := -ldl

wrapper: $(patsubst %.c,%.o,$(wildcard src/wrapper/*.c))
monitor: $(patsubst %.c,%.o,$(wildcard src/monitor/*.c))

$(PROGS):
	$Qecho "  LD      $@";\
	gcc -o $@ $^ $(LDFLAGS)

ldpreload.so: $(filter src/ldpreload/%,$(objs))
	$Qecho "  LD.so   $@";\
	gcc $(CCFLAGS) $(LDFLAGS) -shared -o $@ $^

$(objs): %.o: %.c Makefile
	$Qecho "  CC      $<";\
	gcc -MMD $(CCFLAGS) -Isrc -c $< -o $@ -W -Wall -Werror -Wbad-function-cast -Wcast-align -Wcast-qual -Wchar-subscripts -Winline -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings -fno-common

-include $(deps)
