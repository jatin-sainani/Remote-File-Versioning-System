CC      = gcc
CFLAGS  = -Wall -Wextra -g -pthread

all: server rfs

server: server.o
	$(CC) $(CFLAGS) server.o -o server

rfs: client.o
	$(CC) $(CFLAGS) client.o -o rfs

server.o: server.c config.h
	$(CC) $(CFLAGS) -c server.c

client.o: client.c config.h
	$(CC) $(CFLAGS) -c client.c

clean:
	rm -f server rfs *.o
