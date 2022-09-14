################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Kernel/ahci.c \
../Kernel/avltree.c \
../Kernel/bget.c \
../Kernel/bitops.c \
../Kernel/console.c \
../Kernel/cpu.c \
../Kernel/e1000.c \
../Kernel/elf.c \
../Kernel/ext2.c \
../Kernel/ide.c \
../Kernel/idt.c \
../Kernel/int86.c \
../Kernel/io_dev.c \
../Kernel/irq.c \
../Kernel/keyboard.c \
../Kernel/kthread.c \
../Kernel/main.c \
../Kernel/malloc.c \
../Kernel/mbuf.c \
../Kernel/mm.c \
../Kernel/mouse.c \
../Kernel/page.c \
../Kernel/pci.c \
../Kernel/pid.c \
../Kernel/segment.c \
../Kernel/syscall.c \
../Kernel/time.c \
../Kernel/timer.c \
../Kernel/tss.c \
../Kernel/uio.c \
../Kernel/vesa.c \
../Kernel/vfs.c \
../Kernel/video.c 

ASM_SRCS += \
../Kernel/trap.asm 


OBJS += \
./Kernel/ahci.o \
./Kernel/avltree.o \
./Kernel/bget.o \
./Kernel/bitops.o \
./Kernel/console.o \
./Kernel/cpu.o \
./Kernel/e1000.o \
./Kernel/elf.o \
./Kernel/ext2.o \
./Kernel/ide.o \
./Kernel/idt.o \
./Kernel/int86.o \
./Kernel/io_dev.o \
./Kernel/irq.o \
./Kernel/keyboard.o \
./Kernel/kthread.o \
./Kernel/main.o \
./Kernel/malloc.o \
./Kernel/mbuf.o \
./Kernel/mm.o \
./Kernel/mouse.o \
./Kernel/page.o \
./Kernel/pci.o \
./Kernel/pid.o \
./Kernel/segment.o \
./Kernel/syscall.o \
./Kernel/time.o \
./Kernel/timer.o \
./Kernel/trap.o \
./Kernel/tss.o \
./Kernel/uio.o \
./Kernel/vesa.o \
./Kernel/vfs.o \
./Kernel/video.o 

C_DEPS += \
./Kernel/ahci.d \
./Kernel/avltree.d \
./Kernel/bget.d \
./Kernel/bitops.d \
./Kernel/console.d \
./Kernel/cpu.d \
./Kernel/e1000.d \
./Kernel/elf.d \
./Kernel/ext2.d \
./Kernel/ide.d \
./Kernel/idt.d \
./Kernel/int86.d \
./Kernel/io_dev.d \
./Kernel/irq.d \
./Kernel/keyboard.d \
./Kernel/kthread.d \
./Kernel/main.d \
./Kernel/malloc.d \
./Kernel/mbuf.d \
./Kernel/mm.d \
./Kernel/mouse.d \
./Kernel/page.d \
./Kernel/pci.d \
./Kernel/pid.d \
./Kernel/segment.d \
./Kernel/syscall.d \
./Kernel/time.d \
./Kernel/timer.d \
./Kernel/tss.d \
./Kernel/uio.d \
./Kernel/vesa.d \
./Kernel/vfs.d \
./Kernel/video.d 



# Each subdirectory must supply rules for building sources it contributes
Kernel/%.o: ../Kernel/%.c int86.bin elf_tramp.bin
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc $(CFLAGS) -I. -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '




Kernel/%.o: ../Kernel/%.asm
	@echo 'Building file: $<'
	@echo 'Invoking: Nasm Assembler'
	@ nasm -felf  -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


