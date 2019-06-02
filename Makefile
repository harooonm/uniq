help:
	@echo "targets debug small fast"

clean:
	rm -f btree.o
	rm -f libbtree.so

INCDIR=include/

SRCDIR=src/

SRCS=$(wildcard $(SRCDIR)*.c)

CC=gcc

CFLAGS=-Wall -Wextra -Wfatal-errors -D_POSIX_C_SOURCE=200809L -std=c11

IFLAGS=-I$(INCDIR)

debug:	CFLAGS += -g -Og
fast:	CFLAGS += -O2
small:	CFLAGS += -Os -s -flto

debug:	objs
fast:	objs
small:	objs

objs:
	$(CC) -L$(shell pwd)/lib/ $(CFLAGS) $(IFLAGS) $(SRCS) -o uniq \
		-lbtree
