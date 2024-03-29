CC = gcc
FLAGS = -Wall -Werror -std=gnu11

SERVERFILES = pingserver.c
CLIENTFILES = pingclient.c
DAEMONFILES = daemon.c mac_utils.c mip.c debug.c

CLEANFILES = bin/ping_client bin/ping_server bin/mip_daemon

all: client server daemon
	echo "\n\n\nWARNING: This does not yet work 100%. I have handed in what I have so far.\n\n"

client: $(CLIENTFILES)
	$(CC) $(FLAGS) $(CLIENTFILES) -o bin/ping_client

server: $(SERVERFILES)
	$(CC) $(FLAGS) $(SERVERFILES) -o bin/ping_server

daemon: $(DAEMONFILES)
	$(CC) $(FLAGS) $(DAEMONFILES) -o bin/mip_daemon -lm

clean:
	rm -f $(CLEANFILES)
