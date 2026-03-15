CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11
LDFLAGS ?=
TARGET   = uart_client

SRCS = uart_client.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c include/control.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
