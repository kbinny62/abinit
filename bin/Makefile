
CFLAGS = -std=c89 -pedantic -O2 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=500 -g -ggdb
STRIP ?= strip
MV ?= mv
RM ?= rm 

TARGETS = cat chmod cp cut ln ls mkdir mktemp mv rm rmdir uname unlink
all: $(TARGETS)

clean:
	-$(RM) $(TARGETS)

install: $(TARGETS)
	for i in $(TARGETS); do $(MV) $$i /bin/$$i; $(STRIP) /bin/$$i; done

