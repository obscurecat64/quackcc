CFLAGS=-std=c11 -g -fno-common
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

quackcc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): quackcc.h

test: quackcc
	./test.sh

clean:
	rm -f quackcc *.o *~ tmp*

.PHONY: test clean
