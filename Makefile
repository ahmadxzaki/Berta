APPLICATION = sensor-node
BOARD ?= berta-h10
BASE ?= /home/dav/lora3a-projects
RIOTBASE ?= $(BASE)/RIOT
LORA3ABASE ?= $(BASE)/lora3a-boards

# TRACKER IDENTITY!!!!!
TRACKER_ID ?= 123456
TRACKER_KEY ?= 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF
CFLAGS += -DCONFIG_TRACKER_ID=$(TRACKER_ID)
CFLAGS += -DCONFIG_TRACKER_KEY="$(TRACKER_KEY)"
# ------

# --- External Paths ---
EXTERNAL_BOARD_DIRS  = $(LORA3ABASE)/boards
EXTERNAL_PKG_DIRS    = $(LORA3ABASE)/pkg
# Combine external modules and your local modules
EXTERNAL_MODULE_DIRS = $(LORA3ABASE)/modules $(CURDIR)/modules

# --- Build Config ---
DEVELHELP ?= 1
QUIET ?= 1
PORT ?= /dev/ttyACM0

# --- Hardware Features ---
FEATURES_REQUIRED += periph_cpuid
FEATURES_REQUIRED += periph_adc
FEATURES_REQUIRED += periph_spi_reconfigure
FEATURES_REQUIRED += periph_wdt
FEATURES_REQUIRED += periph_hwrng

USEMODULE += auto_init_random
USEMODULE += random

# --- System & Drivers ---
USEMODULE += xtimer
USEMODULE += ztimer
USEMODULE += ztimer_msec
USEMODULE += stdio_uart
USEMODULE += fmt
USEMODULE += printf_float
USEMODULE += acme_lora

USEMODULE += saml21_backup_mode
USEMODULE += fram

# --- Crypto (Software Optimized) ---
USEMODULE += crypto
USEMODULE += crypto_aes_128
USEMODULE += hashes

# --- Your Application Modules ---
USEMODULE += tracker
USEMODULE += storage
USEMODULE += message_encoding

# --- Configuration ---
# Only keep the region and timeout, as you set BW/SF/Freq in C code
CFLAGS += -DLORA_REGION_US915
CFLAGS += -DSX127X_TX_TIMEOUT_MS=8000

# Debugging (Comment these out for final production build to save space)
CFLAGS += -DLOG_LEVEL=LOG_DEBUG -DSX127X_DEBUG=1

include $(RIOTBASE)/Makefile.include
include $(LORA3ABASE)/Makefile.include