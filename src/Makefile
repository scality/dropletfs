DPL_DIR=../../Droplet/libdroplet

INC_DIR+=$(DPL_DIR)/include
LIB_DIR+=$(DPL_DIR)/lib

CPPFLAGS+=
LDFLAGS+=-ldroplet -lssl -lxml2
FUSE_FLAGS=$(shell pkg-config fuse --cflags --libs)
CFLAGS+=-g -ggdb3 -O0


bin=dplfs

CC=/usr/bin/gcc

SRC=dropletfs.c

all: $(SRC)
	$(CC) -o $(bin) $(SRC)  $(FUSE_FLAGS) $(CPPFLAGS) $(CFLAGS) -I$(INC_DIR) -L$(LIB_DIR) $(LDFLAGS)

clean:
	rm -f *.o *~ $(bin)

