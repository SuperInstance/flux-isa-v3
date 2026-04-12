CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -O2
TEST=test_isa_v3

all: test

test: $(TEST)
	./$(TEST)

$(TEST): $(TEST).c flux-isa-v3.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TEST)

.PHONY: all test clean
