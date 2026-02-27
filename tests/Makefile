# Name of the test application
APPLICATION = test_message_encoding

# By default, compile for your PC, not the microcontroller
BOARD ?= native

# Point to the RIOT base
BASE ?= /home/dav/lora3a-projects
RIOTBASE ?= $(BASE)/RIOT

# Pull in your custom modules
EXTERNAL_MODULE_DIRS = $(CURDIR)/../modules

# 2. Tell the test to compile and link your specific module
USEMODULE += message_encoding

# Use the embUnit testing framework
USEMODULE += embunit

include $(RIOTBASE)/Makefile.include