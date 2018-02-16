CC = gcc
FLAGS = -Wall -Werror -std=gnu11

SERVERFILES = server.c
CLIENTFILES = client.c
DAEMONFILES = daemon.c

CLEANFILES = client server daemon

all: client server daemon

client: $(CLIENTFILES)
	$(CC) $(FLAGS) $(CLIENTFILES) -o client

server: $(SERVERFILES)
	$(CC) $(FLAGS) $(SERVERFILES) -o server

daemon: $(DAEMONFILES)
	$(CC) $(FLAGS) $(DAEMONFILES) -o daemon

clean:
	rm -f $(CLEANFILES)
