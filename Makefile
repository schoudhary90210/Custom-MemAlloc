CC = gcc
CFLAGS = -Wall -g -pthread
LDFLAGS = -pthread

all: mdriver

mdriver: mm.o main.o
	$(CC) $(CFLAGS) -o mdriver mm.o main.o $(LDFLAGS)

mm.o: mm.c mm.h
	$(CC) $(CFLAGS) -c mm.c

main.o: main.c mm.h
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f *.o mdriver
