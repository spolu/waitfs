CC = gcc
CFLAGS = -Wall -g -std=gnu99 -D_FILE_OFFSET_BITS=64
LIBS = -lpthread -lfuse
MAKE = make

WAITFS_SRC = waitfs.c session.c server.c list.c io.c fuse.c

waitfs_obj = $(WAITFS_SRC:.c=.o)

TARGET_WAITFS = ../bin/waitfs

waitfs: $(waitfs_obj)
	$(CC) $(CFLAGS) $(LIBS) -o $(TARGET_WAITFS) $(waitfs_obj)

clean:
	@rm -f $(TARGET_WAITFS) $(waitfs_obj) *~