################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init
	
RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include Boot/subdir.mk
-include lib/subdir.mk
-include Kernel/subdir.mk
-include Kernel/netinet/subdir.mk
-include Kernel/net/subdir.mk
-include Kernel/gfx/subdir.mk
-include objects.mk

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

TARGET_LD = ld
TARGET_OBJCOPY = objcopy

LD_FLAGS := -Map System.map --force-exe-suffix \
	-nostdlib -T../Link.ld

OBJCOPY_FLAGS := -R .dynamic -R .note -R .comment -S -O binary

GENERAL_OPTS := -O0 -Wall 
CC_GENERAL_OPTS := $(GENERAL_OPTS) -Werror -I../Include/

CFLAGS := -c -g -nostdlib -nostdinc -fno-builtin -fno-stack-protector $(CC_GENERAL_OPTS) 

# All Target
all:kernel

kernel:kernel.exe
	@ $(TARGET_OBJCOPY) $(OBJCOPY_FLAGS)  $< $@
	@echo 'Generate binary: '$@' done.'
	@echo ' '

int86.bin:
	@ nasm -fbin -o$@ ../Kernel/int86_trampoline.asm
	@ hexdump -v -e '16/1 "%d," "\n"' int86.bin | sed 's/,,,*/,/' > ./int86_tramp.h

elf_tramp.bin:
	@ nasm -fbin -o$@ ../Kernel/elf_trampoline.asm
	@ hexdump -v -e '16/1 "%d," "\n"' elf_tramp.bin | sed 's/,,,*/,/' > elf_tramp.h


# Tool invocations
kernel.exe: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	@ $(TARGET_LD) $(LD_FLAGS) -o$@ $^
	@echo 'Finished building target: $@'

run:
	@ /opt/qemu/bin/qemu-system-i386 -fda floppy.img -s -boot a -monitor stdio  -drive id=disk,file=hd.disk,if=none -device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0		

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) kernel kernel.exe System.map *.bin
	-@echo ' '


install: 
	@ echo "mounting floppy image..." 
	@ mount -o loop floppy.img /mnt 
	@ echo "copy kernel to floppy..."
	@ \cp kernel /mnt/	
	@	sync
	@ echo "unmounting floppy..."
	@ umount /mnt	
	@ echo "OK"	

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
