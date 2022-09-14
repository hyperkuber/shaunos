################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
ASM_SRCS += \
../Boot/boot.asm 


OBJS += \
./Boot/boot.o 



# Each subdirectory must supply rules for building sources it contributes
Boot/%.o: ../Boot/%.asm
	@echo 'Building file: $<'
	@echo 'Invoking: Nasm Assembler'
	@nasm -felf $< -o$@
	@echo 'Finished building: $<'
	@echo ' '


