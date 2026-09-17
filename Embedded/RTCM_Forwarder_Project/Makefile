# Makefile for rtcm_forwarder (STM32F405R6T6) using ChibiOS 3.0.5

##############################################################################
# Project Metadata
##############################################################################
PROJECT = rtcm_forwarder
CHIBIOS = ../RC_Controller/ChibiOS_3.0.5

MCU = cortex-m4

##############################################################################
# Configuration
##############################################################################
USE_OPT = -O2 -ggdb -fomit-frame-pointer -falign-functions=16 -std=gnu99
USE_OPT += -DBOARD_OTG_NOVBUSSENS -DSTM32F405xx
USE_OPT += -fsingle-precision-constant -Wdouble-promotion
USE_OPT += --specs=nosys.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard
USE_OPT += -DSTM32F4xx_MCUCONF
USE_CPPOPT = -fno-rtti --specs=nosys.specs
USE_LINK_GC = yes
USE_LTO = no
USE_THUMB = yes
USE_VERBOSE_COMPILE = yes
USE_SMART_BUILD = yes
USE_FPU = hard
USE_FWLIB = no

##############################################################################
# Stack and FPU
##############################################################################
USE_PROCESS_STACKSIZE = 0x400
USE_EXCEPTIONS_STACKSIZE = 0x400

##############################################################################
# Imported source files and paths
##############################################################################
include $(CHIBIOS)/os/common/ports/ARMCMx/compilers/GCC/mk/startup_stm32f4xx.mk
include $(CHIBIOS)/os/hal/hal.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F4xx/platform.mk
include $(CHIBIOS)/os/hal/boards/ST_STM32F4_DISCOVERY/board.mk
include $(CHIBIOS)/os/hal/osal/rt/osal.mk
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/rt/ports/ARMCMx/compilers/GCC/mk/port_v7m.mk
include $(CHIBIOS)/os/various/lwip_bindings/lwip.mk

##############################################################################
# Project Sources
##############################################################################
CSRC = $(STARTUPSRC) \
       $(KERNSRC) \
       $(PORTSRC) \
       $(OSALSRC) \
       $(HALSRC) \
       $(PLATFORMSRC) \
       $(BOARDSRC) \
       $(LWSRC) \
       $(CHIBIOS)/os/hal/lib/streams/chprintf.c \
       $(CHIBIOS)/os/various/syscalls.c \
       src/main.c \
       src/rtcm_forward.c \
       src/usbconf.c

INCDIR = $(STARTUPINC) $(KERNINC) $(PORTINC) $(OSALINC) \
         $(HALINC) $(PLATFORMINC) $(BOARDINC) \
         $(LWINC) \
         $(CHIBIOS)/os/various \
         $(CHIBIOS)/os/hal/lib/streams \
         src config

##############################################################################
# Compiler and tools
##############################################################################
TRGT = arm-none-eabi-
CC   = $(TRGT)gcc
CPPC = $(TRGT)g++
LD   = $(TRGT)gcc
CP   = $(TRGT)objcopy
AS   = $(TRGT)gcc -x assembler-with-cpp
AR   = $(TRGT)ar
OD   = $(TRGT)objdump
SZ   = $(TRGT)size
HEX  = $(CP) -O ihex
BIN  = $(CP) -O binary

AOPT =
TOPT = -mthumb -DTHUMB
#CWARN = -Wall -Wextra -Wundef -Wstrict-prototypes
#CPPWARN = -Wall -Wextra -Wundef

UDEFS =
UADEFS =
UINCDIR =
ULIBDIR =
ULIBS = -lm

##############################################################################
# Linker script (adjust if needed)
##############################################################################
LDSCRIPT= $(CHIBIOS)/os/common/ports/ARMCMx/compilers/GCC/ld/stm32f405xg.ld


##############################################################################
# Rules include
##############################################################################
RULESPATH = $(CHIBIOS)/os/common/ports/ARMCMx/compilers/GCC
include $(RULESPATH)/rules.mk
