######################################################################
# Teensy bootloader makefile for win doze
# (c) Adam Munich 2018

# The name of your project (used to name the compiled .hex file)
TARGET = $(notdir $(CURDIR))

# Set to 24000000, 48000000, or 96000000 to set CPU core speed
F_CPU = 96000000

# directory to build in
BUILDDIR = $(abspath $(CURDIR)/build)


######################################################################
# Location of Teensyduino utilities, Toolchain, and Arduino Libraries.

# path location for Teensy Loader, teensy_post_compile and teensy_reboot
TOOLSPATH = $(CURDIR)/tools

# path location for Teensy 3 core
COREPATH = teensy3

# path location for Arduino libraries
LIBRARYPATH = libraries

# Path of project source
SOURCEPATH = src

# path location for the arm-none-eabi compiler
COMPILERPATH = $(TOOLSPATH)/arm/bin

# CPPFLAGS = compiler options for C and C++
CPPFLAGS := -Wall -Wno-sign-compare -Wno-strict-aliasing -g -Os -ffunction-sections
CPPFLAGS += -fdata-sections -nostdlib -D__MK20DX256__ -mcpu=cortex-m4 -mthumb -MMD
CPPFLAGS += -DF_CPU=$(F_CPU) -I$(SOURCEPATH)

# compiler options for C++ only
CXXFLAGS := -std=gnu++14 -felide-constructors -fno-exceptions -fno-rtti

# compiler options for C only
CFLAGS =

# Linker script
LDSCRIPT = $(SOURCEPATH)/mk20dx256.ld

# linker options
LDFLAGS := -T$(LDSCRIPT)
LDFLAGS += -Og -g
LDFLAGS += -mcpu=cortex-m4 -mthumb -lm
LDFLAGS += -ffunction-sections -fdata-sections -nostdlib
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,--defsym,__rtc_localtime=0


######################################################################
# Environment tools

# names for the compiler programs
CC := arm-none-eabi-gcc
CXX := arm-none-eabi-g++
AS := arm-none-eabi-as
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size
DUMP := arm-none-eabi-objdump
JLINK = $(abspath $(TOOLSPATH))/JLink

######################################################################
# Automatically create lists of the sources and objects
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

C_FILES := $(call rwildcard, $(SOURCEPATH), *.c)
CPP_FILES := $(call rwildcard, $(SOURCEPATH), *.cpp)

INC_DIRS := $(sort $(dir $(call rwildcard, $(SOURCEPATH), *)))

######################################################################
# Create file arrays for the c++ and c compiler, and linker

# Include paths for arduino std libraries
LIBRARIES := $(foreach lib, $(INC_DIRS), -I$(lib))

#LIBRARIES := $(foreach lib,$(filter %/, $(wildcard $(LIBRARYPATH)/*/)), -I$(lib))
#LIBRARIES += $(addsuffix /utility, $(LIBRARIES))
#LIBRARIES += $(addsuffix /src, $(LIBRARIES))
#LIBRARIES += $(addsuffix /src/utility, $(LIBRARIES))

# Object file (and assembly file) path array
SOURCE_OBJS := $(C_FILES:.c=.o) $(CPP_FILES:.cpp=.o) $(INO_FILES:.ino=.o)
OBJECTS := $(foreach src, $(SOURCE_OBJS), $(BUILDDIR)/$(src))
OBJECTS += $(LS_FILES:.S=.o)


######################################################################
# Create build rules

all: build

build: $(TARGET).hex $(TARGET).bin $(TARGET).asm
	@echo. & echo."Done!"
    
reboot:
	@-$(abspath $(TOOLSPATH))/teensy_reboot

#upload: post_compile reboot
upload: $(TARGET).asm
	$(JLINK) -if swd -device MK20DX256xxx7 -autoconnect 1 -speed 1000 -CommanderScript "$(abspath $(CURDIR)/scripts)/upload.jlink"
	#-CommanderScript=C:\xxxxxx.jlink

$(BUILDDIR)/%.o: %.c
	@echo. & echo."Building file $(notdir $<)"
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(LIBRARIES) -c "$<" -o "$@" 

$(BUILDDIR)/%.o: %.cpp
	@echo. & echo."Building file $(notdir $<)"
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LIBRARIES) -c "$<" -o "$@"

$(BUILDDIR)/%.o: %.S
	@echo & echo.Building file $(notdir $<)
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(CXX) -x assembler-with-cpp $(CPPFLAGS) $(CXXFLAGS) $(LIBRARIES) -c "$<" -o "$@"

$(TARGET).elf: $(OBJECTS)
	@echo. & echo."Linking $(notdir $<)"
	@$(CXX) $(LDFLAGS) $^ -o "$(BUILDDIR)/$@" 

%.hex: %.elf
	@echo. & echo."Making HEX from $(notdir $<)"
	@$(OBJCOPY) -O ihex -R .eeprom "$(BUILDDIR)/$<" "$(BUILDDIR)/$@"

%.bin: %.elf
	@echo. &echo."Making BIN from $(notdir $<)"
	@$(SIZE) "$(BUILDDIR)/$<"
	@$(OBJCOPY) -O binary "$(BUILDDIR)/$<" "$(BUILDDIR)/$@"

%.asm: %.elf
	@echo. & echo."Dumping $(notdir $<)" 
	@$(DUMP) -d -S "$(BUILDDIR)/$<" > "$(BUILDDIR)/$(TARGET)_elf.asm"
	@$(DUMP) -marm -Mforce-thumb -d -S "$(BUILDDIR)/$<" > "$(BUILDDIR)/$(TARGET)_bin.asm"

clean:
	@echo Cleaning...
	rmdir /s /q "$(abspath $(BUILDDIR))"
	del "*.hex" "*.bin" "*.elf"