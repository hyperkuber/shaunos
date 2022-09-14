################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Kernel/gfx/draw.c \
../Kernel/gfx/frame.c \
../Kernel/gfx/gfx_main.c \
../Kernel/gfx/bitmap.c	\
../Kernel/gfx/kframe.c	\
../Kernel/gfx/window.c


OBJS += \
./Kernel/gfx/draw.o \
./Kernel/gfx/frame.o \
./Kernel/gfx/gfx_main.o \
./Kernel/gfx/text.o	\
./Kernel/gfx/font_8x16.o	\
./Kernel/gfx/blit.o	\
./Kernel/gfx/bitmap.o	\
./Kernel/gfx/kframe.o	\
./Kernel/gfx/window.o


C_DEPS += \
./Kernel/gfx/draw.d \
./Kernel/gfx/frame.d \
./Kernel/gfx/gfx_main.d \
./Kernel/gfx/text.d	\
./Kernel/gfx/font_8x16.d \
./Kernel/gfx/blit.d	\
./Kernel/gfx/bitmap.d	\
./Kernel/gfx/kframe.d	\
./Kernel/gfx/window.d


# Each subdirectory must supply rules for building sources it contributes
Kernel/gfx/%.o: ../Kernel/gfx/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc $(CFLAGS) -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


