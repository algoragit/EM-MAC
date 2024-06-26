# Replace with correct path to contiki root directory
CONTIKI = $(HOME)/contiki-3.x

APPS = powertrace collect-view
CONTIKI_PROJECT = udp-sender udp-sink
PROJECT_SOURCEFILES += collect-common.c

ifdef PERIOD
CFLAGS=-DPERIOD=$(PERIOD)
endif

all: $(CONTIKI_PROJECT)

CONTIKI_WITH_IPV6 = 1

include $(CONTIKI)/Makefile.include


.PHONY: show-modules
show-modules:
	@echo $(MODULES) | tr ' ' '\n'

.PHONY: show-sourcefiles
show-sourcefiles: | $(CONTIKI_SOURCEFILES)
	@echo $| | tr ' ' '\n' | sort

minimal-project.c:
	@echo '#include "contiki.h"' > minimal-project.c
	@echo 'AUTOSTART_PROCESSES(NULL);' >> minimal-project.c

.PHONY: clean-minimal-project
clean-minimal-project:
	@rm -f minimal-project.c minimal-project.$(TARGET)

# required for process substitution
SHELL := /bin/bash

# this creates the minimal-project firmware and shows the linked sourcefiles for that firmware
.PHONY: show-linked-sourcefiles
show-linked-sourcefiles: $(CONTIKI_PROJECT).$(TARGET) | $(CONTIKI_SOURCEFILES)
	@LINKED_FILES=$$(grep -wFf <($(NM) $(CONTIKI_PROJECT).$(TARGET) | awk '{ print $$3 }') \
	                           <($(NM) --defined-only --extern-only --print-file-name contiki-$(TARGET).a) | \
	                 gawk 'match($$0, /^contiki-$(TARGET).a:(.*).o:/, f) { print f[1] ".c" }' | sort | uniq); \
	grep -wFf <(echo $${LINKED_FILES} | tr ' ' '\n') <(echo $| | tr ' ' '\n') | sort

# builds the specified firmware and shows the linked sourcefiles for that firmware
.PHONY: %.show-linked-sourcefiles
%.show-linked-sourcefiles: %.$(TARGET) | $(CONTIKI_SOURCEFILES)
	@LINKED_FILES=$$(grep -wFf <($(NM) $< | awk '{ print $$3 }') \
	                           <($(NM) --defined-only --extern-only --print-file-name contiki-$(TARGET).a) | \
	                 gawk 'match($$0, /^contiki-$(TARGET).a:(.*).o:/, f) { print f[1] ".c" }' | sort | uniq); \
	grep -wFf <(echo $${LINKED_FILES} | tr ' ' '\n') <(echo $| | tr ' ' '\n') | sort

.PHONY: %.preprocessed
%.preprocessed: %.c
	@$(CC) -E -dD -dI -P -C $(CFLAGS) $<

.PHONY: %.macros
%.macros: %.c
	@$(CC) -E -dM $(CFLAGS) $<

.PHONY: %.dependencies
%.dependencies: $(OBJECTDIR)/%.o contiki-$(TARGET).a
	@grep -wFf <($(NM) --undefined-only $< | awk '{ print $$2 }') \
	           <($(NM) --defined-only --print-file-name contiki-$(TARGET).a) | \
	sort | gawk 'match($$0, /^contiki-$(TARGET).a:(.*).o:/, f) { printf "%-30s %-30s\n", f[1], $$3 }'
	@SYMBOLS=$$($(NM) --undefined-only $< | awk '{ print $$2 }'); \
	for SYMBOL in $${SYMBOLS}; do \
	  if ! grep -qw $${SYMBOL} <($(NM) --defined-only --print-file-name contiki-$(TARGET).a); \
	  then \
	    printf "%-30s %-30s\n" "?" "$${SYMBOL}"; \
	  fi; \
	done

.PHONY: %.dependents
%.dependents: $(OBJECTDIR)/%.o contiki-$(TARGET).a
	@grep -wFf <($(NM) --defined-only $< | awk '{ print $$3 }') \
	           <($(NM) --undefined-only --print-file-name contiki-$(TARGET).a) | \
	sort | gawk 'match($$0, /^contiki-$(TARGET).a:(.*).o:/, f) { printf "%-30s %-30s\n", f[1], $$3 }'

.PHONY: graph-dependencies
graph-dependencies: $(CONTIKI_PROJECT).$(TARGET)
	@DOT_FILE=graph-dependencies-$(TARGET).dot; \
	LINKED_FILES=$$(grep -wFf <($(NM) $< | awk '{ print $$3 }') \
	                          <($(NM) --defined-only --extern-only --print-file-name contiki-$(TARGET).a) | \
	                gawk 'match($$0, /^contiki-$(TARGET).a:(.*).o:/, f) { print f[1] }' | sort | uniq); \
	\
	function dependencies { \
	  grep -wFf <($(NM) --undefined-only $(OBJECTDIR)/$$1.o | awk '{ print $$2 }') \
	            <($(NM) --defined-only --print-file-name contiki-$(TARGET).a) | \
	  sort | gawk 'match($$0, /^contiki-$(TARGET).a:(.*).o:/, f) { print f[1] }' | uniq; \
	}; \
	\
	echo "digraph {" > $${DOT_FILE}; \
	echo "outputorder=edgesfirst" >> $${DOT_FILE}; \
	echo "edge[color=\"LightGrey\"]" >> $${DOT_FILE}; \
	\
	for MODULE in $(MODULES); do \
	  echo "subgraph \"cluster_$${MODULE}\" {" >> $${DOT_FILE}; \
	  echo "label = \"$${MODULE}\"" >> $${DOT_FILE}; \
	  for MODULE_FILE in $$(find $(CONTIKI)/$${MODULE} -maxdepth 1 -name '*.c' -exec basename {} ".c" \;); do \
	    if [[ $${LINKED_FILES} =~ $${MODULE_FILE} ]]; then \
	      echo "  \"$${MODULE_FILE}\";" >> $${DOT_FILE}; \
	    fi \
	  done; \
	  echo "}" >> $${DOT_FILE}; \
	done; \
	\
	for NODE in $${LINKED_FILES}; do \
	  DEPENDENCIES=$$(dependencies $${NODE}); \
	  for DEPENDENCY in $${DEPENDENCIES}; do \
	    echo "  \"$${NODE}\" -> \"$${DEPENDENCY}\"" >> $${DOT_FILE}; \
	  done \
	done; \
	echo "}" >> $${DOT_FILE}