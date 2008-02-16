srcs := $(wildcard *.c)
objs := $(srcs:.c=.o)
deps := $(srcs:.c=.d)

ldpreload.so: $(objs)
	@echo "  LD.so   $@";\
	gcc -fPIC -shared -o $@ -ldl $^

$(objs): %.o: %.c Makefile
	@echo "  CC      $<";\
	gcc -fPIC -MMD -c $< -o $@ -W -Wall -Werror -Wbad-function-cast -Wcast-align -Wcast-qual -Wchar-subscripts -Winline -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings -fno-common

-include $(deps)
