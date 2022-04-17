#
#
LDFLAGS=	# -lnsl -lsocket

all: client

client: client.c
	gcc -o client client.c -ansi -pedantic -Wall -std=c17

clean:
	-rm -fr client server
