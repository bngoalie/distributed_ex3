CC=gcc
LD=gcc
CFLAGS = -c -std=c99 -Wall -Wextra -pedantic -g -D_GNU_SOURCE
CPPFLAGS=-I. -I/home/cs437/exercises/ex3/include
SP_LIBRARY_DIR=/home/cs437/exercises/ex3

all: mcast 

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

mcast:  $(SP_LIBRARY_DIR)/libspread-core.a mcast.o
	$(LD) -o $@ mcast.o $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

clean:
	rm -f *.o mcast

