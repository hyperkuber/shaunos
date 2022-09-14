GENERAL_OPTS := -O0 -Wall 
CC_GENERAL_OPTS := $(GENERAL_OPTS) -Werror -I../../Include/

CFLAGS := -c -g -nostdlib -nostdinc -fno-builtin -fno-stack-protector $(CC_GENERAL_OPTS) \
		 

C_SRCS += \
	if_ethersubr.c	\
	if_le.c	\
	if.c	\
	netisr.c	\
	route.c	\
	rtsock.c	\
	raw_usrreq.c	\
	raw_cb.c

OBJS += \
	if_ethersubr.o	\
	if_le.o	\
	if.o	\
	netisr.o	\
	route.o	\
	radix.o	\
	rtsock.o	\
	raw_usrreq.o	\
	raw_cb.o	\
	uipc_socket2.o	\
	uipc_socket.o	\
	uipc_domain.o	\
	uipc_syscall.o
	
INETDIR = ../netinet/	

INET_OBJS += \
	../netinet/check_sum.o \
	../netinet/ip_icmp.o	\
	../netinet/ip_igmp.o	\
	../netinet/ip_input.o	\
	../netinet/ip_output.o	\
	../netinet/in.o	\
	../netinet/raw_ip.o	\
	../netinet/if_ether.o	\
	../netinet/in_pcb.o	\
	../netinet/udp_usrreq.o	\
	../netinet/tcp_input.o \
	../netinet/tcp_output.o	\
	../netinet/tcp_subr.o	\
	../netinet/tcp_timer.o	\
	../netinet/tcp_usrreq.o	\
	../netinet/in_proto.o	\
	../netinet/dhcp.o
	
	
all:net.o

%.o: %.c
	@gcc -S $(CC_GENERAL_OPTS) $<
	gcc $(CFLAGS)  -o$@ $<




net.o:$(OBJS) $(INET_OBJS)
	@ ld -r -o"$@" $^



clean: 
	rm -rf *.o *~ *.s *.bin *.h
	rm -rf ../netinet/*.o
depend:
	gcc -MM -I../Include *.c | sed 's/\.c/.c/g' >> Makefile

