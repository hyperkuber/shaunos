################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../lib/string.c \
../lib/unistd.c \
../lib/vsprintf.c 

OBJS += \
./lib/string.o \
./lib/unistd.o \
./lib/vsprintf.o 

C_DEPS += \
./lib/string.d \
./lib/unistd.d \
./lib/vsprintf.d 


# Each subdirectory must supply rules for building sources it contributes
lib/%.o: ../lib/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc $(CFLAGS) -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


