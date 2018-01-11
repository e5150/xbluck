PREFIX ?= /usr/local
LIBS = xcb-randr xcb-keysyms xkbcommon
CPPFLAGS += -I. -D_DEFAULT_SOURCE -D_GNU_SOURCE -D_XOPEN_SOURCE $(shell pkg-config --cflags $(LIBS))
LDFLAGS  += -L.
LDLIBS   += -lm -lcrypt $(shell pkg-config --libs $(LIBS))
CFLAGS   += -g --std=c99 -fpic -O2 -Wall -Wextra -pedantic

.SUFFIXES:
.SUFFIXES: .o .c

SRC = auth.c filter.c main.c options.c util.c xcb.c

OBJ = $(SRC:.c=.o)
PRG = xbluck

all: $(PRG)

$(PRG): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

config.h:
	cat config.def.h > $@

options.o: config.h
$(OBJ): xbluck.h

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<


install: all
	install $(PRG) -D -t $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f $(OBJ) $(PRG)

i: install

c: clean

.PHONY:
	all depend install clean dist i c
