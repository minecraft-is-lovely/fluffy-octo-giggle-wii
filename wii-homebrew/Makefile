#---------------------------------------------------------------------------------
# Wii Homebrew Makefile
# Requires devkitPPC + libogc from devkitPro: https://devkitpro.org
#
# Targets:
#   make          — build boot.dol and boot.elf
#   make clean    — remove build output
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
# devkitPro path — override by setting DEVKITPRO in your environment,
# or by installing via the pacman-based devkitPro installer.
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "DEVKITPRO is not set. \
Export DEVKITPRO=/opt/devkitpro (or wherever devkitPro is installed) \
and re-run make.")
endif

ifeq ($(strip $(DEVKITPPC)),)
export DEVKITPPC := $(DEVKITPRO)/devkitPPC
endif

include $(DEVKITPPC)/wii_rules

#---------------------------------------------------------------------------------
# Target name — the final output will be $(TARGET).dol / $(TARGET).elf
#---------------------------------------------------------------------------------
TARGET		:= boot
BUILD		:= build
SOURCES		:= source
INCLUDES	:= include

#---------------------------------------------------------------------------------
# Options for code generation
#---------------------------------------------------------------------------------
CFLAGS		= -g -O2 -Wall -Wextra $(MACHDEP) $(INCLUDE)
CXXFLAGS	= $(CFLAGS)
LDFLAGS		= -g $(MACHDEP) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# Libraries
#   -lwiiuse   — Wii remote input
#   -lbte      — Bluetooth stack (required by wiiuse)
#   -logc      — libogc core (video, memory, syscalls)
#   -lm        — math library
#---------------------------------------------------------------------------------
LIBS		:= -lwiiuse -lbte -logc -lm

#---------------------------------------------------------------------------------
# No implicit rules — list all source files explicitly for clarity
#---------------------------------------------------------------------------------
CFILES		:= $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:= $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:= $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:= $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))

#---------------------------------------------------------------------------------
# Use CXX for linking C++ projects, CC for C-only projects
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD := $(CC)
else
	export LD := $(CXX)
endif

export OFILES_SOURCES	:= $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
						   $(sFILES:.s=.o) $(SFILES:.S=.o)
export OFILES			:= $(OFILES_SOURCES)
export HFILES			:= $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE			:= $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
						   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
						   -I$(CURDIR)/$(BUILD) \
						   -I$(LIBOGC_INC)

export LIBPATHS			:= $(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
						   -L$(LIBOGC_LIB)

.PHONY: all clean

#---------------------------------------------------------------------------------
all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

#---------------------------------------------------------------------------------
clean:
	@echo "Cleaning build output..."
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).dol
	@echo "Done."

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
# In the build subdirectory: compile and link
#---------------------------------------------------------------------------------

DEPENDS		:= $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
$(TARGET).dol: $(TARGET).elf
$(TARGET).elf: $(OFILES)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
