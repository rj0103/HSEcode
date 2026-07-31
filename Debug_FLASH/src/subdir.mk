################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/APP_Main.c \
../src/HSE_FlashStorage_Example.c \
../src/HSE_Mac_Ecc_Example.c \
../src/HSE_Main.c \
../src/HSE_SecureBoot.c \
../src/HSE_TestDummySmrInstall.c \
../src/LED.c \
../src/UART_Print.c \
../src/main.c 

OBJS += \
./src/APP_Main.o \
./src/HSE_FlashStorage_Example.o \
./src/HSE_Mac_Ecc_Example.o \
./src/HSE_Main.o \
./src/HSE_SecureBoot.o \
./src/HSE_TestDummySmrInstall.o \
./src/LED.o \
./src/UART_Print.o \
./src/main.o 

C_DEPS += \
./src/APP_Main.d \
./src/HSE_FlashStorage_Example.d \
./src/HSE_Mac_Ecc_Example.d \
./src/HSE_Main.d \
./src/HSE_SecureBoot.d \
./src/HSE_TestDummySmrInstall.d \
./src/LED.d \
./src/UART_Print.d \
./src/main.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	arm-none-eabi-gcc "@src/APP_Main.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


