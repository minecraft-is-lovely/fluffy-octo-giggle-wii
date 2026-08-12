#---------------------------------------------------------------------------------
# A Takeover — Wii Homebrew
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "Set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

include $(DEVKITPPC)/wii_rules

#---------------------------------------------------------------------------------
TARGET   := boot
BUILD    := build
SOURCES  := source
INCLUDES := include

#---------------------------------------------------------------------------------
CFILES   := $(foreach d,$(SOURCES),$(notdir $(wildcard $(d)/*.c)))
OFILES   := $(CFILES:.c=.o)

CFLAGS   := -g -O2 -Wall $(MACHDEP) \
             $(foreach d,$(INCLUDES),-iquote $(CURDIR)/$(d)) \
             -I$(LIBOGC_INC)
LDFLAGS  := -g $(MACHDEP) -Wl,-Map,$(TARGET).map
LIBS     := -lwiiuse -lbte -logc -lm
LIBPATHS := -L$(LIBOGC_LIB)

vpath %.c $(SOURCES)

.PHONY: all clean

all: $(TARGET).dol

$(TARGET).dol: $(TARGET).elf
	elf2dol $< $@
	@echo "built $(notdir $@)"

$(TARGET).elf: $(addprefix $(BUILD)/,$(OFILES))
	@echo "linking $(notdir $@)"
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBPATHS) $(LIBS)

$(BUILD)/%.o: %.c | $(BUILD)
	@echo "compiling $<"
	$(CC) -MMD -MP -MF $(BUILD)/$*.d $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).dol $(TARGET).map

-include $(wildcard $(BUILD)/*.d)
