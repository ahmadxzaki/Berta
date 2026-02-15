APPLICATION = sensor-node
BOARD ?= berta-h10
BASE ?= $(CURDIR)/../../..
LORA3ABASE ?= $(BASE)/lora3a-boards
RIOTBASE ?= $(BASE)/RIOT
EXTERNAL_BOARD_DIRS=$(LORA3ABASE)/boards
EXTERNAL_MODULE_DIRS=$(LORA3ABASE)/modules
EXTERNAL_PKG_DIRS=$(LORA3ABASE)/pkg
DEVELHELP ?= 1
QUIET ?= 1
PORT ?= /dev/ttyUSB0

USEMODULE += fmt
USEMODULE += hdc3020
USEMODULE += periph_adc
USEMODULE += periph_cpuid
USEMODULE += periph_spi_reconfigure
USEMODULE += printf_float
USEMODULE += saml21_backup_mode
USEMODULE += stdio_uart
USEMODULE += xtimer
USEMODULE += ztimer
USEMODULE += ztimer_msec

USEMODULE += acme_lora

ifneq ($(BW),)
	CFLAGS+=-DDEFAULT_LORA_BANDWIDTH=$(BW)
endif

ifneq ($(SF),)
	CFLAGS+=-DDEFAULT_LORA_SPREADING_FACTOR=$(SF)
endif

ifneq ($(CR),)
	CFLAGS+=-DDEFAULT_LORA_CODERATE=$(CR)
endif

ifneq ($(CH),)
	CFLAGS+=-DDEFAULT_LORA_CHANNEL=$(CH)
endif

ifneq ($(PW),)
	CFLAGS+=-DDEFAULT_LORA_POWER=$(PW)
endif

CFLAGS += -DLORA_REGION_US915
CFLAGS += -DDEFAULT_LORA_BOOST=SX127X_PA_BOOST
CFLAGS += -DLOG_LEVEL=LOG_DEBUG -DSX127X_DEBUG=1
# allow long packets plenty of time
CFLAGS += -DSX127X_TX_TIMEOUT_MS=8000

include $(RIOTBASE)/Makefile.include
include $(LORA3ABASE)/Makefile.include