################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../Project4_SampleTests/mailbox.o \
../Project4_SampleTests/testmailbox1.o \
../Project4_SampleTests/testmailbox2.o \
../Project4_SampleTests/testmailbox3.o \
../Project4_SampleTests/testmailbox4.o \
../Project4_SampleTests/testmailbox5.o \
../Project4_SampleTests/testmailbox6.o \
../Project4_SampleTests/testmailbox7.o 

C_SRCS += \
../Project4_SampleTests/mailbox.c \
../Project4_SampleTests/testmailbox1.c \
../Project4_SampleTests/testmailbox2.c \
../Project4_SampleTests/testmailbox3.c \
../Project4_SampleTests/testmailbox4.c \
../Project4_SampleTests/testmailbox5.c \
../Project4_SampleTests/testmailbox6.c \
../Project4_SampleTests/testmailbox7.c 

OBJS += \
./Project4_SampleTests/mailbox.o \
./Project4_SampleTests/testmailbox1.o \
./Project4_SampleTests/testmailbox2.o \
./Project4_SampleTests/testmailbox3.o \
./Project4_SampleTests/testmailbox4.o \
./Project4_SampleTests/testmailbox5.o \
./Project4_SampleTests/testmailbox6.o \
./Project4_SampleTests/testmailbox7.o 

C_DEPS += \
./Project4_SampleTests/mailbox.d \
./Project4_SampleTests/testmailbox1.d \
./Project4_SampleTests/testmailbox2.d \
./Project4_SampleTests/testmailbox3.d \
./Project4_SampleTests/testmailbox4.d \
./Project4_SampleTests/testmailbox5.d \
./Project4_SampleTests/testmailbox6.d \
./Project4_SampleTests/testmailbox7.d 


# Each subdirectory must supply rules for building sources it contributes
Project4_SampleTests/%.o: ../Project4_SampleTests/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


