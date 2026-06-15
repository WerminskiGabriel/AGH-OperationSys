CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread -lrt -lm

.PHONY: all clean

all: main

main: main.c
	$(CC) $(CFLAGS) -o main main.c $(LDFLAGS)

clean:
	rm -f main *.o
	rm -rf output
