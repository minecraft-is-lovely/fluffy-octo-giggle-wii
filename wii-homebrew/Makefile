#---------------------------------------------------------------------------------
# Wii Homebrew Makefile — works with devkitPro wii-dev toolchain
# https://devkitpro.org
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "Set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

# Wii-specific rules (compiler flags, elf2dol, etc.)
include $(DEVKITPRO)/wii_rules

#---------------------------------------------------------------------------------
# Target / directories
#---------------------------------------------------------------------------------
TARGET   := boot
BUILD    := build
SOURCES  := source
INCLUDES := include

#---------------------------------------------------------------------------------
# Compiler flags
#---------------------------------------------------------------------------------
CFLAGS   := -g -O2 -Wall $(MACHDEP) $(INCLUDE)
CXXFLAGS := $(CFLAGS)
LDFLAGS  := -g $(MACHDEP) -Wl,-Map,$(notdir $@.map)

#---------------------------------------------------------------------------------
# Libraries  (-lwiiuse -lbte must come before -logc)
#---------------------------------------------------------------------------------
LIBS     := -lwiiuse -lbte -logc -lm

#---------------------------------------------------------------------------------
# Source files
#---------------------------------------------------------------------------------
CFILES   := $(foreach d,$(SOURCES),$(notdir $(wildcard $(d)/*.c)))
CPPFILES := $(foreach d,$(SOURCES),$(notdir $(wildcard $(d)/*.cpp)))
sFILES   := $(foreach d,$(SOURCES),$(notdir $(wildcard $(d)/*.s)))
SFILES   := $(foreach d,$(SOURCES),$(notdir $(wildcard $(d)/*.S)))

ifeq ($(strip $(CPPFILES)),)
  export LD := $(CC)
else
  export LD := $(CXX)
endif

export OFILES   := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export INCLUDE  := $(foreach d,$(INCLUDES),-iquote $(CURDIR)/$(d)) \
                   $(foreach d,$(LIBDIRS),-I$(d)/include) \
                   -I$(CURDIR)/$(BUILD) \
                   -I$(LIBOGC_INC)
export LIBPATHS := $(foreach d,$(LIBDIRS),-L$(d)/lib) -L$(LIBOGC_LIB)

.PHONY: all clean

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).dol

else
#---------------------------------------------------------------------------------
DEPENDS := $(OFILES:.o=.d)

$(TARGET).dol: $(TARGET).elf
$(TARGET).elf: $(OFILES)

-include $(DEPENDS)
endif
