CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -pedantic -O2
TARGET   = ringbuf-test

.PHONY: all test clean

all: $(TARGET)

$(TARGET): ringbuf.c ringbuf.h test/test_ringbuf.c
	$(CC) $(CFLAGS) -o $@ ringbuf.c test/test_ringbuf.c

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o
