CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra
LDLIBS   = -lX11 -lXrender -lXext -lm
PREFIX  ?= $(HOME)/.local

laser-ink: laser-ink.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

install: laser-ink
	install -Dm755 laser-ink $(DESTDIR)$(PREFIX)/bin/laser-ink
	install -Dm644 contrib/laser-ink.desktop $(DESTDIR)$(PREFIX)/share/applications/laser-ink.desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/laser-ink
	rm -f $(DESTDIR)$(PREFIX)/share/applications/laser-ink.desktop

clean:
	rm -f laser-ink

.PHONY: install uninstall clean
