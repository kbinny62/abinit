
CFLAGS ?= -O2 -s
STRIP ?= strip
MV ?= mv

TARGETS = env id which
all: $(TARGETS)



.phony: clean
clean:
	-unlink $(TARGETS)

install: $(TARGETS)
	for i in $(TARGETS); do $(MV) $$i /usr/bin ; $(STRIP) /usr/bin/$$i; done
