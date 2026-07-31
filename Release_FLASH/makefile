################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/subdir.mk
-include generate/src/subdir.mk
-include board/subdir.mk
-include RTD/src/subdir.mk
-include Project_Settings/Startup_Code/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_FLASH += \
GSLU_APP.srec \

SECONDARY_SIZE += \
GSLU_APP.siz \


# All Target
all: GSLU_APP.elf secondary-outputs

# Tool invocations
GSLU_APP.elf: $(OBJS) G:\cw_HSE\workspace\GSLU_APP\GSLU_APP\Project_Settings\Linker_Files\linker_flash_s32k312_Release.ld $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Standard S32DS C Linker'
	arm-none-eabi-gcc -o "GSLU_APP.elf" "@GSLU_APP.args"  $(USER_OBJS)
	@echo 'Finished building target: $@'
	@echo ' '

GSLU_APP.srec: GSLU_APP.elf
	@echo 'Invoking: Standard S32DS Create Flash Image'
	arm-none-eabi-objcopy -O srec GSLU_APP.elf  "GSLU_APP.srec"
	@echo 'Finished building: $@'
	@echo ' '

GSLU_APP.siz: GSLU_APP.elf
	@echo 'Invoking: Standard S32DS Print Size'
	arm-none-eabi-size --format=berkeley GSLU_APP.elf
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) ./*
	-@echo ' '

secondary-outputs: $(SECONDARY_FLASH) $(SECONDARY_SIZE)

.PHONY: all clean dependents

-include ../makefile.targets
