CC=gcc
LDFLAGS=-lpthread
CFLAGS=-c -Wall

all: server client

server: server.o
	$(CC) $(LDFLAGS) server.o -o server.exe

server.o: server.c
	$(CC) $(CFLAGS) main.c

client: client.o
	$(CC) $(LDFLAGS) client.o -o client.exe

client.o: client.c
	$(CC) $(CFLAGS) main.c


clean:
	rm *.o server.exe client.exe server client
