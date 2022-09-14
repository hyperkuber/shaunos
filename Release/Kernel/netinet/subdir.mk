################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Kernel/netinet/check_sum.c \
../Kernel/netinet/dhcp.c \
../Kernel/netinet/if_ether.c \
../Kernel/netinet/in.c \
../Kernel/netinet/in_pcb.c \
../Kernel/netinet/in_proto.c \
../Kernel/netinet/ip_icmp.c \
../Kernel/netinet/ip_igmp.c \
../Kernel/netinet/ip_input.c \
../Kernel/netinet/ip_output.c \
../Kernel/netinet/raw_ip.c \
../Kernel/netinet/tcp_input.c \
../Kernel/netinet/tcp_output.c \
../Kernel/netinet/tcp_subr.c \
../Kernel/netinet/tcp_timer.c \
../Kernel/netinet/tcp_usrreq.c \
../Kernel/netinet/udp_usrreq.c 

OBJS += \
./Kernel/netinet/check_sum.o \
./Kernel/netinet/dhcp.o \
./Kernel/netinet/if_ether.o \
./Kernel/netinet/in.o \
./Kernel/netinet/in_pcb.o \
./Kernel/netinet/in_proto.o \
./Kernel/netinet/ip_icmp.o \
./Kernel/netinet/ip_igmp.o \
./Kernel/netinet/ip_input.o \
./Kernel/netinet/ip_output.o \
./Kernel/netinet/raw_ip.o \
./Kernel/netinet/tcp_input.o \
./Kernel/netinet/tcp_output.o \
./Kernel/netinet/tcp_subr.o \
./Kernel/netinet/tcp_timer.o \
./Kernel/netinet/tcp_usrreq.o \
./Kernel/netinet/udp_usrreq.o 

C_DEPS += \
./Kernel/netinet/check_sum.d \
./Kernel/netinet/dhcp.d \
./Kernel/netinet/if_ether.d \
./Kernel/netinet/in.d \
./Kernel/netinet/in_pcb.d \
./Kernel/netinet/in_proto.d \
./Kernel/netinet/ip_icmp.d \
./Kernel/netinet/ip_igmp.d \
./Kernel/netinet/ip_input.d \
./Kernel/netinet/ip_output.d \
./Kernel/netinet/raw_ip.d \
./Kernel/netinet/tcp_input.d \
./Kernel/netinet/tcp_output.d \
./Kernel/netinet/tcp_subr.d \
./Kernel/netinet/tcp_timer.d \
./Kernel/netinet/tcp_usrreq.d \
./Kernel/netinet/udp_usrreq.d 



# Each subdirectory must supply rules for building sources it contributes
Kernel/netinet/%.o: ../Kernel/netinet/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc $(CFLAGS) -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


