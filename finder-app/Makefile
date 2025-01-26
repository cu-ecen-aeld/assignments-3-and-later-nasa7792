ifdef CROSS_COMPILE
      CC=$(CROSS_COMPILE)gcc
else
    CC=gcc
endif

all: writer

writer: writer.c
	$(CC) -o writer writer.c

.PHONY: clean
clean:
	rm -f *.o
