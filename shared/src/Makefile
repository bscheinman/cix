CC=clang
OBJECTS=book.o		\
	event.o		\
	heap.o		\
	vector.o

CFLAGS=-Wall -Werror -g -O0 -I/opt/backtrace/include
INCLUDES=-I../include

all: $(OBJECTS) ../include/*.h

book.o: book.c ../include/book.h
	$(CC) $(INCLUDES) book.c $(CFLAGS) -c

event.o: event.c ../include/event.h
	$(CC) $(INCLUDES) event.c $(CFLAGS) -c

heap.o: heap.c ../include/heap.h
	$(CC) $(INCLUDES) heap.c $(CFLAGS) -c

vector.o: vector.c ../include/vector.h
	$(CC) $(INCLUDES) vector.c $(CFLAGS) -c
