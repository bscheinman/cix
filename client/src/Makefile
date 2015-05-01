include ../../cix.mk

.PHONY:all clean

OBJECTS=client.o

INCLUDES=-I../include -I../../shared/include
LDFLAGS=-pthread
SHARED_LIBS=../../shared/src
SHARED_OBJS=	$(SHARED_LIBS)/buffer.o		\
		$(SHARED_LIBS)/event.o		\
		$(SHARED_LIBS)/heap.o		\
		$(SHARED_LIBS)/id_generator.o	\
		$(SHARED_LIBS)/vector.o		\
		$(SHARED_LIBS)/worq.o

all: stress

stress: stress.c $(OBJECTS)
	$(CC) $(INCLUDES) $(CFLAGS) -o stress $(SHARED_OBJS) $(OBJECTS) stress.c $(LDFLAGS)

client.o: client.c ../include/*.h
	$(CC) $(INCLUDES) client.c $(CFLAGS) -c

clean:
	rm stress *.o
