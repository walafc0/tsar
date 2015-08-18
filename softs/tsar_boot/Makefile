# let the user have a default configuration (ie for HARD_CONFIG_PATH)
-include ./build.mk

USE_DT           ?= 0
DTS              ?= platform.dts
HARD_CONFIG_PATH ?= ../../../giet_vm
MAKECMDGOALS     ?= none

ifneq ($(MAKECMDGOALS),$(filter $(MAKECMDGOALS),clean distclean clean-doc doc))
  ifndef HARD_CONFIG_PATH
    $(error please define HARD_CONFIG_PATH 'make HARD_CONFIG_PATH=foo')
  else
    $(info Make for $(HARD_CONFIG_PATH))
  endif
endif

# Platform clock frequency (in KHz)
ifdef SYSTEM_CLK
    DEFS   := "-DRESET_SYSTEM_CLK=$(SYSTEM_CLK)"
endif

LD         := mipsel-unknown-elf-ld
CC         := mipsel-unknown-elf-gcc
AS         := mipsel-unknown-elf-as
DU         := mipsel-unknown-elf-objdump
AR         := mipsel-unknown-elf-ar
RM         := rm -rf
SED        := sed
ECHO       := @echo
MKDIR      := mkdir
DTC        := dtc
HEXDUMP    := hexdump
DOXYGEN    := doxygen

BUILD_DIR  := build
HARD_CONFIG:= $(HARD_CONFIG_PATH)/hard_config.h

# =============================================================================
# Include files paths
# =============================================================================

INCLUDE    += -I. -Iinclude -Idrivers -I$(HARD_CONFIG_PATH)

# =============================================================================
# Object files
# =============================================================================

VPATH      += src

CFLAGS     := -Wall                \
              -mno-gpopt           \
              -ffreestanding       \
              -fomit-frame-pointer \
              -march=mips32        \
              -O2                  \
              -Werror

C_SRCS     := reset_elf_loader.c \
              reset_utils.c      \
              reset_exception.c  \
              reset_ioc.c        \
              version.c

S_SRCS     := reset.S

OBJS       := $(addprefix $(BUILD_DIR)/,\
                  $(subst .c,.o, $(notdir $(C_SRCS))) \
                  $(subst .S,.o, $(notdir $(S_SRCS))))

TARGET     := preloader.elf

# =============================================================================
# Drivers library
# =============================================================================

VPATH      += drivers

DRV_SRCS   := reset_tty.c        \
              reset_inval.c      \
              reset_ioc_spi.c    \
              reset_ioc_bdv.c    \
              reset_ioc_rdk.c    \
              reset_ioc_hba.c    \
              reset_ioc_sdc.c    \
              spi.c

DRV_OBJS   := $(addprefix $(BUILD_DIR)/,\
                  $(subst .c,.o, $(notdir $(DRV_SRCS))))

DRV_LIB    := libdrivers.a

# =============================================================================
# Makefile rules
# =============================================================================

VPATH      += $(BUILD_DIR)

all: $(TARGET)

$(DRV_OBJS): $(HARD_CONFIG)
$(OBJS): $(HARD_CONFIG)

$(BUILD_DIR)/$(DRV_LIB): $(BUILD_DIR) $(DRV_OBJS)
	$(ECHO) "[   AR    ]     $(notdir $@)"
	$(AR) rcs $@ $(DRV_OBJS)

$(BUILD_DIR)/version.o: $(BUILD_DIR)/version.c
	$(ECHO) "[   CC    ]     $(notdir $<)"
	$(CC) $(DEFS) $(CFLAGS) $(INCLUDE) -c -o $@ $<
	$(DU) -D $@ > $@.txt

$(BUILD_DIR)/version.c: $(BUILD_DIR) version.sh VERSION
	$(ECHO) "[version.sh]"
	./version.sh > $@

$(BUILD_DIR)/preloader.ld: preloader.ld.in
	$(ECHO) "[   CC    ]     $(notdir $<)"
	$(CC) -x c -P -E -DUSE_DT=$(USE_DT) $(INCLUDE) $< -o $@

$(TARGET): $(BUILD_DIR) $(OBJS) $(BUILD_DIR)/preloader.ld $(BUILD_DIR)/$(DRV_LIB)
	$(ECHO) "[   LD    ]     $@"
	$(LD) -o $@ -T $(BUILD_DIR)/preloader.ld $(OBJS) -L$(BUILD_DIR) -ldrivers
	$(DU) -D $@ > $@.txt

ifeq ($(USE_DT), 1)
$(TARGET): $(BUILD_DIR)/platform.ld

$(BUILD_DIR)/platform.dtb: $(HARD_CONFIG_PATH)/$(DTS)
	$(ECHO) "[   DTC   ]     $(notdir $<)"
	$(DTC) -O dtb -o $@ $< > /dev/null 2>&1

$(BUILD_DIR)/platform.ld: $(BUILD_DIR)/platform.dtb
	$(ECHO) "[ HEXDUMP ]     $(notdir $<)"
	$(HEXDUMP) -v -e '"BYTE(0x" 1/1 "%02X" ")\n"' $< > $@
endif

$(BUILD_DIR):
	$(MKDIR) $@

doc: Doxyfile
	$(DOXYGEN) Doxyfile

clean:
	$(RM) $(TARGET).txt $(TARGET) *~ $(BUILD_DIR)

clean-doc:
	$(RM) doc

distclean: clean clean-doc

# =============================================================================
# Implicit makefile rules

$(BUILD_DIR)/%.o: %.c
	$(ECHO) "[   CC    ]     $(notdir $<)"
	$(CC) $(DEFS) $(CFLAGS) $(INCLUDE) -c -o $@ $<
	$(DU) -D $@ > $@.txt

$(BUILD_DIR)/%.o: %.S
	$(ECHO) "[   AS    ]     $(notdir $<)"
	$(CC) $(DEFS) ${INCLUDE} -g -mips32 -c -o $@ $<
	$(DU) -D $@ > $@.txt

