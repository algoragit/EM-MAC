CONTIKI = ../..
APPS = powertrace collect-view
CONTIKI_PROJECT = udp-sender udp-sink
PROJECT_SOURCEFILES += collect-common.c

CONTIKI_WITH_RIME = 1
DEFINES = PROJECT_CONF_H=\"project-conf.h\"

MODULES += core/net/mac/muchmac

ifdef PERIOD
CFLAGS=-DPERIOD=$(PERIOD)
endif

all: $(CONTIKI_PROJECT)

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
