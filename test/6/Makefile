WRAPPER := ../../wrapper

CFLAGS ?= -O2 -pipe

# Loads of nice flags to ensure our code is good
# IMPORTANT: We should be using c99 instead of gnu99 but for some reason
# generic linux headers as of 2.6.19 don't allow this in asm/types.h
CFLAGS += -pedantic -std=gnu99 \
    -Wall -Wunused -Wimplicit -Wshadow -Wformat=2 \
    -Wmissing-declarations -Wno-missing-prototypes -Wwrite-strings \
    -Wbad-function-cast -Wnested-externs -Wcomment -Winline \
    -Wchar-subscripts -Wcast-align -Wno-format-nonliteral

# We define _BSD_SOURCE as GNU supports BSD too - which is nice :)
CDEFS = -D_BSD_SOURCE

dhcpcd_H = version.h
dhcpcd_OBJS = arp.o client.o common.o configure.o dhcp.o dhcpcd.o duid.o \
		info.o interface.o ipv4ll.o logger.o signals.o socket.o

dhcpcd: $(dhcpcd_H) $(dhcpcd_OBJS)
	$(WRAPPER) $(CC) $(LDFLAGS) $(dhcpcd_OBJS) $(LIBRESOLV) -o dhcpcd

version.h:
	echo '#define VERSION "test"' > version.h

$(dhcpcd_OBJS): 
	$(WRAPPER) $(CC) $(CDEFS) $(CFLAGS) -c $*.c

clean:
	rm -rf $(TARGET) $(dhcpcd_H) *.o .tup
