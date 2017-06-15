
CFLAGS = -std=c89 -pedantic -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=500 -O2 -g -ggdb
RM ?= rm

TARGETS = env id sort which
all: $(TARGETS)



.phony: clean
clean:
	-$(RM) -f $(TARGETS)

install: $(TARGETS)
	for i in $(TARGETS); do $(MV) $$i /usr/bin ; $(STRIP) /usr/bin/$$i; done
