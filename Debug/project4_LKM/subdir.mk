################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../project4_LKM/Mailbox_LKM.mod.o \
../project4_LKM/Mailbox_LKM.o 

C_SRCS += \
../project4_LKM/Mailbox_LKM.c \
../project4_LKM/Mailbox_LKM.mod.c 

OBJS += \
./project4_LKM/Mailbox_LKM.o \
./project4_LKM/Mailbox_LKM.mod.o 

C_DEPS += \
./project4_LKM/Mailbox_LKM.d \
./project4_LKM/Mailbox_LKM.mod.d 


# Each subdirectory must supply rules for building sources it contributes
project4_LKM/%.o: ../project4_LKM/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


