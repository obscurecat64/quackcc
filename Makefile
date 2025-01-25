CFLAGS=-std=c11 -g -fno-common

quackcc: main.o
	$(CC) -o quackcc main.o $(LDFLAGS)

test: quackcc
	./test.sh

clean:
	rm -f quackcc *.o *~ tmp*

.PHONY: test clean