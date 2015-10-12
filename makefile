CC=gcc
LDFLAGS=-pthread
CFLAGS=-c -Wall

all: server client

server: server.o
	$(CC) $(LDFLAGS) server.o -o server.exe

server.o: server.c
	$(CC) $(CFLAGS) server.c

client: client.o
	$(CC) $(LDFLAGS) client.o -o client.exe

client.o: client.c
	$(CC) $(CFLAGS) client.c


clean:
	rm *.o server.exe client.exe server client
