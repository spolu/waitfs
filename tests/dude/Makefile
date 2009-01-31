CC = gcc
CFLAGS = -Wall -g -std=gnu99 
LIBS = 
MAKE = make

DUDE_SRC = dude.c io.c

dude_obj = $(DUDE_SRC:.c=.o)

TARGET_DUDE = ../../bin/dude

dude: $(dude_obj) 
	$(CC) $(CFLAGS) $(LIBS) -o $(TARGET_DUDE) $(dude_obj)

clean:
	@rm -f $(TARGET_DUDE) $(dude_obj) *~