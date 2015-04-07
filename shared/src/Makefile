CC=clang
OBJECTS=event.o		\
	id_generator.o	\
	heap.o		\
	vector.o	\
	worq.o

CFLAGS=-Wall -Werror -g -O0 -I/opt/backtrace/include
INCLUDES=-I../include

all: $(OBJECTS) ../include/*.h

event.o: event.c ../include/event.h
	$(CC) $(INCLUDES) event.c $(CFLAGS) -c

id_generator.o: id_generator.c ../include/id_generator.h
	$(CC) $(INCLUDES) id_generator.c $(CFLAGS) -c

heap.o: heap.c ../include/heap.h
	$(CC) $(INCLUDES) heap.c $(CFLAGS) -c

vector.o: vector.c ../include/vector.h
	$(CC) $(INCLUDES) vector.c $(CFLAGS) -c

worq.o: worq.c ../include/worq.h
	$(CC) $(INCLUDES) worq.c $(CFLAGS) -c

clean:
	rm *.o
