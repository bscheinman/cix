CC=clang
OBJECTS=event.o

CFLAGS=-Wall -Werror -g
INCLUDES=-I../include

all: $(OBJECTS) ../include/*.h

event.o:
	$(CC) $(INCLUDES) event.c $(CFLAGS) -c
