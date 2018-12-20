all: traffic

CFLAGS=-std=gnu99 -Wall -g

traffic: traffic.o cars.o
	gcc $(CFLAGS) -pthread -o $@ $^

%.o : %.c traffic.h
	gcc $(CFLAGS) -c $<

clean : 
	rm -f *.o traffic *~ results.txt

