CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -O2 -D_GNU_SOURCE
LDFLAGS :=

TARGET := cpman

SRC := cpman.c
HEADER := cpman.h

all: $(TARGET)

$(TARGET): $(SRC) $(HEADER)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin

uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall

