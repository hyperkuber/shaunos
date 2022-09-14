GENERAL_OPTS := -O0 -Wall 
CC_GENERAL_OPTS := $(GENERAL_OPTS) -Werror -I../../Include/

CFLAGS := -c -g -nostdlib -nostdinc -fno-builtin -fno-stack-protector $(CC_GENERAL_OPTS) \
		 

C_SRCS += \
	draw.c	\
	gfx_main.c	\
	frame.c	\
	kframe.c	\
	window.c

OBJS += \
	draw.o	\
	frame.o	\
	gfx_main.o	\
	text.o	\
	font_8x16.o	\
	blit.o	\
	kframe.o	\
	window.o
	
	
all:gfx.o

%.o: %.c
	@gcc -S $(CC_GENERAL_OPTS) $<
	gcc $(CFLAGS)  -o$@ $<




gfx.o:$(OBJS)
	@ ld -r -o"$@" $^



clean: 
	rm -rf *.o *~ *.s *.bin 
depend:
	gcc -MM -I../Include *.c | sed 's/\.c/.c/g' >> Makefile

