DPL_DIR=../../Droplet/libdroplet

DPL_INC_DIR=$(DPL_DIR)/include
DPL_LIB_DIR=$(DPL_DIR)/lib

DPL_CFLAGS=-I$(DPL_INC_DIR) -L$(DPL_LIB_DIR)

FUSE_CFLAGS=$(shell pkg-config fuse --cflags)
FUSE_LDFLAGS=$(shell pkg-config fuse --libs)

GLIB_CFLAGS=$(shell pkg-config --cflags glib-2.0)
GLIB_LDFLAGS=$(shell pkg-config --libs glib-2.0)

CPPFLAGS+=
LDFLAGS+=-ldroplet -lssl -lxml2 $(FUSE_LDFLAGS) $(GLIB_LDFLAGS) -L$(DPL_LIB_DIR)
CFLAGS+=-g -ggdb3 -O0 $(FUSE_CFLAGS) $(GLIB_CFLAGS) $(DPL_CFLAGS)

SRC=$(wildcard *.c)
OBJ= $(SRC:.c=.o)

bin=dplfs

CC=/usr/bin/gcc

all: $(bin)

dplfs: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) *~ $(bin)

