
CFLAGS ?= -std=c99 -O2 -g -ggdb
STRIP ?= strip
MV ?= mv
RM ?= unlink

TARGETS = cat chmod cp cut ln ls mkdir mktemp mv rm rmdir uname unlink
all: $(TARGETS)

.phony: clean
clean:
	-$(RM) $(TARGETS)

install: $(TARGETS)
	for i in $(TARGETS); do $(MV) $$i /bin/$$i; $(STRIP) /bin/$$i; done

