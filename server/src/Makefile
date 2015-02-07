CC=clang
OBJECTS=market.o	\
	session.o

CFLAGS=-Wall -Werror -g -O0
INCLUDES=-I../include -I../../shared/include -I/opt/backtrace/include
LDFLAGS=-pthread
SHARED_LIBS=../../shared/src
SHARED_OBJS=	$(SHARED_LIBS)/book.o		\
		$(SHARED_LIBS)/event.o		\
		$(SHARED_LIBS)/heap.o		\
		$(SHARED_LIBS)/vector.o

all: server.o $(OBJECTS)
	$(CC) $(INCLUDES) $(CFLAGS) -o cix_server $(SHARED_OBJS) $(OBJECTS) server.o $(LDFLAGS)

market.o: market.c ../include/*.h
	$(CC) $(INCLUDES) market.c $(CFLAGS) -c

server.o: server.c ../include/*.h
	$(CC) $(INCLUDES) server.c $(CFLAGS) -c

session.o: session.c ../include/*.h
	$(CC) $(INCLUDES) session.c $(CFLAGS) -c
