CROSS_COMPILE ?=

CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Werror

TARGET := writer
SRC := writer.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

.PHONY: all clean