#use flags for wall and werror all warnings will be treated as errors
#yocto ld flags added
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
TARGET ?= aesdsocket
LDFLAGS ?= -lpthread -lrt
all: $(TARGET)
$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

.PHONY: clean
clean:
	-rm -f $(TARGET)