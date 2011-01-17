DPL_INC_DIR=/usr/local/include/droplet-1.0
DPL_LIB_DIR=/usr/local/lib

DPL_CFLAGS=-I$(DPL_INC_DIR) -L$(DPL_LIB_DIR)

FUSE_CFLAGS=$(shell pkg-config fuse --cflags)
FUSE_LDFLAGS=$(shell pkg-config fuse --libs)

GLIB_CFLAGS=$(shell pkg-config --cflags glib-2.0)
GLIB_LDFLAGS=$(shell pkg-config --libs glib-2.0)

CPPFLAGS+=
LDFLAGS+=-ldroplet -lssl -lxml2 $(FUSE_LDFLAGS) $(GLIB_LDFLAGS) -L$(DPL_LIB_DIR)

CFLAGS+=-Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
CFLAGS+=-Wall -Werror
CFLAGS+=-Wformat-nonliteral -Wcast-align -Wpointer-arith
CFLAGS+=-Wbad-function-cast -Wmissing-prototypes -Wstrict-prototypes
CFLAGS+=-Wmissing-declarations
CFLAGS+=-Winline -Wundef -Wnested-externs
# CFLAGS+=-Wcast-qual
# CFLAGS+=-Wshadow
CFLAGS+=-Wconversion
# CFLAGS+=-Wwrite-strings
CFLAGS+=-Wno-conversion -Wfloat-equal -Wuninitialized
CFLAGS+=-g -ggdb3 -O2

CFLAGS+=$(FUSE_CFLAGS) $(GLIB_CFLAGS) $(DPL_CFLAGS)

SRC=$(wildcard src/*.c)
OBJ= $(SRC:.c=.o)

DEST=/usr/local/bin

bin=bin/dplfs

CC=/usr/bin/gcc

all: $(bin)

bin/dplfs: $(OBJ)
	mkdir -p bin
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clear:
	@echo -n "removing the trailing spaces in source files... "
	@find . -name \*.h -o -name \*.c -exec sed -i 's:\s\+$$::g' {} \;
	@echo "done"

clean:
	rm -f $(OBJ) *~ $(bin)

uninstall:
	rm -f $(DEST)/$(bin)

install:
	install -m755 $(bin) $(DEST)
