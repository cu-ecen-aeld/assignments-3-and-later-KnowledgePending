CROSS_COMPILE ?=

CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Werror -Wpedantic -O3

TARGET := aesdsocket
SRC := aesdsocket.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

.PHONY: all clean