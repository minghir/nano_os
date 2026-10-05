CC := gcc
CFLAGS := -O2

SRC := nan2hdd.c nan3hdd.c psf_to_c.c
BIN := $(SRC:.c=)

all: $(BIN)

nan2hdd: nan2hdd.c
	$(CC) $(CFLAGS) $< -o $@

nan3hdd: nan3hdd.c
	$(CC) $(CFLAGS) $< -o $@
	
psf_to_c: psf_to_c.c
	$(CC) $(CFLAGS) $< -o $@


clean:
	rm -f $(BIN)