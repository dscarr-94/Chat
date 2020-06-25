# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2017
# Modified by Dylan Carr April 2020
# dscarr94@gmail.com

CC= gcc
CFLAGS= -g -Wall
LIBS =


all:   cclient server

cclient: cclient.c networks.o pollLib.o gethostbyname6.o packets.o *.h
	$(CC) $(CFLAGS) -o cclient cclient.c networks.o pollLib.o gethostbyname6.o packets.o $(LIBS)

server: server.c networks.o pollLib.o gethostbyname6.o packets.o *.h
	$(CC) $(CFLAGS) -o server server.c networks.o pollLib.o gethostbyname6.o packets.o $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f server cclient *.o
