BIN := slimemold

CFLAGS := -Wall -Wextra -Werror
ifeq ($(D), 1)
CFLAGS +=  -g -O0
else
CFLAGS +=  -O2
endif

all: $(BIN)

slimemold: slimemold.c slimemold_simulation.c util.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -f $(BIN)
