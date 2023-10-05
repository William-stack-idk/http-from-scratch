CC = gcc
CFLAGS = -Wall -Wextra -g

all: server

myserver: server.c
	$(CC) $(CFLAGS) -o server server.c

clean:
	rm -f server
