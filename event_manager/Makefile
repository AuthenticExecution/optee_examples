CFLAGS = -Wall -DDBG
SRCS=$(wildcard host/*.c)
OBJS=$(SRCS:.c=.o)

PORT ?= 1236

%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

all: $(OBJS)
	gcc $(CFLAGS) -o app $(OBJS)

run: all
	./app $(PORT)

clean:
	rm -rf $(OBJS) app