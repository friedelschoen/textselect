CFLAGS += -g -std=c99 -O2 -Wall -Wextra -Wpedantic
LDFLAGS += -lncurses
PREFIX ?= /usr

BINS := textselect pipeto
HEADERS := arg.h config.h

.PHONY: all install clean

all: $(BINS)

%.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

%: %.o
	$(CC) $< -o $@ $(LDFLAGS)

pipeto: CPPFLAGS += -D_GNU_SOURCE

install: all
	cp textselect $(PREFIX)/bin/
	cp pipeto $(PREFIX)/bin/
	cp textselect.1 $(PREFIX)/share/man/man1/

clean:
	rm -f textselect textselect.o pipeto pipeto.o
