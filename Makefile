CC = gcc
FLAGS = -Wall -Werror -std=gnu11

SERVERFILES = pingserver.c
CLIENTFILES = pingclient.c
DAEMONFILES = daemon.c shared.c mac_utils.c

CLEANFILES = bin/ping_client bin/ping_server bin/mip_daemon

all: client server daemon

client: $(CLIENTFILES)
	$(CC) $(FLAGS) $(CLIENTFILES) -o bin/ping_client

server: $(SERVERFILES)
	$(CC) $(FLAGS) $(SERVERFILES) -o bin/ping_server

daemon: $(DAEMONFILES)
	$(CC) $(FLAGS) $(DAEMONFILES) -o bin/mip_daemon

clean:
	rm -f $(CLEANFILES)
