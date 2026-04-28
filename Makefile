CC      ?= gcc
PG_CFG  ?= pg_config
CFLAGS   = -Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE \
           -Iinclude -I$(shell $(PG_CFG) --includedir)
LDFLAGS  = -L$(shell $(PG_CFG) --libdir) -lpq -lm

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)
BIN  = pg_autoindex

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(BIN)

.PHONY: all clean
