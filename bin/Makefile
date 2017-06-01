
CFLAGS ?= -O2 -g -ggdb
STRIP ?= strip
MV ?= mv
RM ?= unlink

TARGETS = cat cut ln ls mkdir mktemp mv rmdir uname unlink
all: $(TARGETS)

.phony: clean
clean:
	-$(RM) $(TARGETS)

install: $(TARGETS)
	for i in $(TARGETS); do $(MV) $$i /bin/$$i; $(STRIP) /bin/$$i; done

