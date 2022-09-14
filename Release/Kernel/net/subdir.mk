################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Kernel/net/if.c \
../Kernel/net/if_ethersubr.c \
../Kernel/net/if_le.c \
../Kernel/net/netisr.c \
../Kernel/net/radix.c \
../Kernel/net/raw_cb.c \
../Kernel/net/raw_usrreq.c \
../Kernel/net/route.c \
../Kernel/net/rtsock.c \
../Kernel/net/uipc_domain.c \
../Kernel/net/uipc_socket.c \
../Kernel/net/uipc_socket2.c \
../Kernel/net/uipc_syscall.c 

OBJS += \
./Kernel/net/if.o \
./Kernel/net/if_ethersubr.o \
./Kernel/net/if_le.o \
./Kernel/net/netisr.o \
./Kernel/net/radix.o \
./Kernel/net/raw_cb.o \
./Kernel/net/raw_usrreq.o \
./Kernel/net/route.o \
./Kernel/net/rtsock.o \
./Kernel/net/uipc_domain.o \
./Kernel/net/uipc_socket.o \
./Kernel/net/uipc_socket2.o \
./Kernel/net/uipc_syscall.o 

C_DEPS += \
./Kernel/net/if.d \
./Kernel/net/if_ethersubr.d \
./Kernel/net/if_le.d \
./Kernel/net/netisr.d \
./Kernel/net/radix.d \
./Kernel/net/raw_cb.d \
./Kernel/net/raw_usrreq.d \
./Kernel/net/route.d \
./Kernel/net/rtsock.d \
./Kernel/net/uipc_domain.d \
./Kernel/net/uipc_socket.d \
./Kernel/net/uipc_socket2.d \
./Kernel/net/uipc_syscall.d 


# Each subdirectory must supply rules for building sources it contributes
Kernel/net/%.o: ../Kernel/net/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc $(CFLAGS) -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


