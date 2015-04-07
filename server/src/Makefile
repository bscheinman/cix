CC=clang
OBJECTS=book.o		\
	market.o	\
	session.o	\
	trade_log.o

CFLAGS=-Wall -Werror -g -O0 -fcolor-diagnostics
INCLUDES=-I../include -I../../shared/include
LDFLAGS=-pthread
SHARED_LIBS=../../shared/src
SHARED_OBJS=	$(SHARED_LIBS)/event.o		\
		$(SHARED_LIBS)/heap.o		\
		$(SHARED_LIBS)/id_generator.o	\
		$(SHARED_LIBS)/vector.o		\
		$(SHARED_LIBS)/worq.o

all: server.o $(OBJECTS)
	$(CC) $(INCLUDES) $(CFLAGS) -o cix_server $(SHARED_OBJS) $(OBJECTS) server.o $(LDFLAGS)

book.o: book.c ../include/*.h
	$(CC) $(INCLUDES) book.c $(CFLAGS) -c

market.o: market.c ../include/*.h
	$(CC) $(INCLUDES) market.c $(CFLAGS) -c

server.o: server.c ../include/*.h
	$(CC) $(INCLUDES) server.c $(CFLAGS) -c

session.o: session.c ../include/*.h
	$(CC) $(INCLUDES) session.c $(CFLAGS) -c

trade_log.o: trade_log.c ../include/*.h
	$(CC) $(INCLUDES) trade_log.c $(CFLAGS) -c

clean:
	rm cix_server *.o
