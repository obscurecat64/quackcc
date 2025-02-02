CFLAGS=-std=c11 -g -fno-common -Wall
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

quackcc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c quackcc.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: quackcc
	./test.sh

clean:
	rm -f quackcc *.o *~ tmp*

.PHONY: test clean
