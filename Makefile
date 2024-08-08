CFLAGS += -g -std=c99 -O2 -Wall -Wextra -Wpedantic
LDFLAGS += -lncurses
PREFIX ?= /usr

.PHONY: all install clean

all: textselect

textselect.o: textselect.c arg.h
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

textselect: textselect.o
	$(CC) $< -o $@ $(LDFLAGS)

install: textselect textselect.1
	cp textselect $(PREFIX)/bin/
	cp textselect.1 $(PREFIX)/share/man/man1/

clean:
	rm -f textselect textselect.o
