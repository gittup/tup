srcs := $(wildcard *.c)
objs := $(srcs:.c=.o)
deps := $(srcs:.c=.d)

PROG_NAME := monitor

all: $(objs)
	@echo "  LD      $(PROG_NAME)";\
	gcc $^ -o $(PROG_NAME)

$(objs): %.o: %.c Makefile
	@echo "  CC      $<";\
	gcc -MMD -c $< -o $@ -W -Wall -Werror -Wbad-function-cast -Wcast-align -Wcast-qual -Wchar-subscripts -Winline -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings -fno-common

-include $(deps)
