CFLAGS += -O2 -Wall -Wextra -Wpedantic -g
LDFLAGS += -lncurses
PREFIX = /usr

.PHONY: all install clean

all: textselect

textselect.o: textselect.c arg.h
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

textselect: textselect.o
	$(CC) $< -o $@ $(LDFLAGS)

install: textselect textselect.1
	cp textselect $(PREFIX)/bin/
	cp textselect.1 $(PREFIX)/share/man/man1/

clean:
	rm -f textselect textselect.o
