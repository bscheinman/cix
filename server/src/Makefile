CC=clang
OBJECTS=../../shared/src/event.o	\
	session.o

CFLAGS=-Wall -Werror -g
INCLUDES=-I../include -I../../shared/include
LDFLAGS=-pthread

all: server.o $(OBJECTS)
	$(CC) $(INCLUDES) $(CFLAGS) -o cix_server $(OBJECTS) server.o $(LDFLAGS)

server.o:
	$(CC) $(INCLUDES) server.c $(CFLAGS) -c

session.o:
	$(CC) $(INCLUDES) session.c $(CFLAGS) -c
