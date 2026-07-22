
# --------------------------------------------------------------------------
#
#	FILE NAME:		Makefile.globals
#     AUTHOR:
#	DESCRIPTION:
#
#
# --------------------------------------------------------------
PLATFORM=jetson-nano
##Ideally required if doing cross compilation so that proper config can be picked up
RPI_MODEL=2
# Assign unique number to each Target type
TW=1
SPI=2

#Assign unique number to each HBI
I2C=1

ifeq ($(TARGET),)
	TARGET=TW
endif
HBI=I2C
HBI_ENABLE_PROCFS=1

# These also can be input from Make command line. For now,
# hardcode or if user wish to change can make change here
HOST_ENDIAN=little
VPROC_DEV_ENDIAN=big
BOOT_FROM_HOST=yes
FLASH_PRESENT=yes
BUILD_TYPE=DEBUG
HBI_MAX_INST_PER_DEV=3
VPROC_MAX_NUM_DEVS=1
VPROC_DEV_NAME_SIZE=32
NUM_MAX_LOCKS=100
HBI_BUFFER_SIZE=1024
DEBUG_LEVEL=15
export SHELL=/bin/bash

# Set this to yes, if you want ton compile in the example ALSA codec mixer
# no, otherwise
#VPROC_CODEC_MIXER_ENABLE=no
VPROC_CODEC_MIXER_ENABLE=yes

# Enable UBT code
#UBT_CODE=no
UBT_CODE=yes

# Set these options to 1 if you want to
# Compile in Firmware/config images statically?
# yes, no
HBI_LOAD_FWR_STATIC=no
HBI_LOAD_CFGREC_STATIC=no

# IF this is enabled linux/firmware.h header is compiled in
# in order to support bootloading of *.bin at boot
HBI_ENABLE_FWR_BIN=yes

# ------------------------------------------------------------------------------
# Location of cross compiled Toolchain, Linux, and libs.
# ------------------------------------------------------------------------------



#files for global defines

MSCC_SND_COD_MOD =snd-soc-zl380xx
MSCC_SND_MAC_MOD =snd-soc-microsemi-dac
MSCC_HBI_MOD =hbi
MSCC_SND_MIXER_MOD =snd-soc-zl380xx-mixer

MSCC_DAC_OVERLAY_DTB =microsemi-dac-overlay
MSCC_SPIMULTI_DTB =microsemi-spi-multi-tw-overlay
MSCC_SPI_DTB =microsemi-spi-overlay

EXTRA_CFLAGS += -D$(BUILD_TYPE) -DDEBUG_LEVEL=$(DEBUG_LEVEL)
EXTRA_LDFLAGS += -zmuldefs

ifneq ($(TARGET),)
	EXTRA_CFLAGS += -DTARGET=$(TARGET)
ifeq ($(TARGET),TW)
	EXTRA_CFLAGS += -DTW=$(TW)
endif
endif

ifneq ($(HBI),)
	EXTRA_CFLAGS += -DHBI=$(HBI) -DI2C=$(I2C) -DSPI=$(SPI)
ifeq ($(HBI),I2C)
	EXTRA_CFLAGS += -DI2C=$(I2C)
endif
ifeq ($(HBI),SPI)
	EXTRA_CFLAGS += -DSPI=$(SPI)
endif
endif
ifeq ($(BOOT_FROM_HOST),yes)
	EXTRA_CFLAGS += -DBOOT_FROM_HOST
endif

ifeq ($(FLASH_PRESENT),yes)
	EXTRA_CFLAGS += -DFLASH_PRESENT
endif

ifeq ($(VPROC_DEV_ENDIAN),little)
	EXTRA_CFLAGS += -DVPROC_DEV_ENDIAN_LITTLE=1
else
	EXTRA_CFLAGS += -DVPROC_DEV_ENDIAN_LITTLE=0
endif

ifeq ($(HOST_ENDIAN),little)
	EXTRA_CFLAGS += -DHOST_ENDIAN_LITTLE=1
else
	EXTRA_CFLAGS += -DHOST_ENDIAN_LITTLE=0
endif

ifneq ($(HBI_MAX_INST_PER_DEV),)
	EXTRA_CFLAGS += -DHBI_MAX_INST_PER_DEV=$(HBI_MAX_INST_PER_DEV)
endif

ifneq ($(HBI_MAX_INSTANCES),)
	EXTRA_CFLAGS += -DHBI_MAX_INSTANCES=$(HBI_MAX_INSTANCES)
endif

ifneq ($(VPROC_MAX_NUM_DEVS),)
	EXTRA_CFLAGS += -DVPROC_MAX_NUM_DEVS=$(VPROC_MAX_NUM_DEVS)
endif

ifneq ($(VPROC_DEV_NAME_SIZE),)
	EXTRA_CFLAGS += -DVPROC_DEV_NAME_SIZE=$(VPROC_DEV_NAME_SIZE)
endif

ifneq ($(NUM_MAX_LOCKS),)
	EXTRA_CFLAGS += -DNUM_MAX_LOCKS=$(NUM_MAX_LOCKS)
endif

ifneq ($(HBI_BUFFER_SIZE),)
	EXTRA_CFLAGS += -DHBI_BUFFER_SIZE=$(HBI_BUFFER_SIZE)
endif

ifneq ($(CHIP),)
	EXTRA_CFLAGS += -DCHIP=$(CHIP)
endif
EXTRA_CFLAGS += -DHBI_ENABLE_PROCFS=$(HBI_ENABLE_PROCFS)

ifneq ($(VPROC_DEV_INT_MODE_OD),)
	EXTRA_CFLAGS += -DVPROC_DEV_INT_MODE_OD
endif

ifeq ($(HBI_LOAD_CFGREC_STATIC),yes)
	EXTRA_CFLAGS += -DHBI_LOAD_CFGREC_STATIC=1
else
	EXTRA_CFLAGS += -DHBI_LOAD_CFGREC_STATIC=0
endif

ifeq ($(HBI_LOAD_FWR_STATIC),yes)
	EXTRA_CFLAGS += -DHBI_LOAD_FWR_STATIC=1
else
	EXTRA_CFLAGS += -DHBI_LOAD_FWR_STATIC=0
endif

ifeq ($(HBI_ENABLE_FWR_BIN),yes)
	EXTRA_CFLAGS += -DHBI_ENABLE_FWR_BIN=1
else
	EXTRA_CFLAGS += -DHBI_ENABLE_FWR_BIN=0
endif

ifeq ($(VPROC_CODEC_MIXER_ENABLE),yes)
	EXTRA_CFLAGS += -DVPROC_CODEC_MIXER_ENABLE=1
else
	EXTRA_CFLAGS += -DVPROC_CODEC_MIXER_ENABLE=0
endif

ifeq ($(UBT_CODE), yes)
	EXTRA_CFLAGS += -DUBT_CODE=1
else
	EXTRA_CFLAGS += -DUBT_CODE=0
endif

CFG_FILE_NAME :=`basename $(CFG_C_FILE) .h`
