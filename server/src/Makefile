include ../../cix.mk

.PHONY:all clean

OBJECTS=book.o		\
	market.o	\
	session.o	\
	trade_log.o

INCLUDES=-I../include -I../../shared/include
LDFLAGS=-pthread
SHARED_LIBS=../../shared/src
SHARED_OBJS=	$(SHARED_LIBS)/buffer.o		\
		$(SHARED_LIBS)/event.o		\
		$(SHARED_LIBS)/heap.o		\
		$(SHARED_LIBS)/id_generator.o	\
		$(SHARED_LIBS)/vector.o		\
		$(SHARED_LIBS)/worq.o

all: cix_server log_viewer

cix_server: server.o $(OBJECTS)
	$(CC) $(INCLUDES) $(CFLAGS) -o cix_server $(SHARED_OBJS) $(OBJECTS) server.o $(LDFLAGS)

log_viewer: log_viewer.c trade_log.o ../include/*.h
	$(CC) $(INCLUDES) $(CFLAGS) -o log_viewer $(SHARED_OBJS) trade_log.o log_viewer.c $(LDFLAGS)

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
