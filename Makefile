CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic

LIBOBJ = src/isa.o src/asm.o src/core.o src/util.o
PROGS  = tasm tsim tdis ttest

all: $(PROGS)

tasm: src/tasm.o $(LIBOBJ)
	$(CC) $(CFLAGS) -o $@ src/tasm.o $(LIBOBJ)

tsim: src/tsim.o $(LIBOBJ)
	$(CC) $(CFLAGS) -o $@ src/tsim.o $(LIBOBJ)

tdis: src/tdis.o $(LIBOBJ)
	$(CC) $(CFLAGS) -o $@ src/tdis.o $(LIBOBJ)

ttest: src/ttest.o $(LIBOBJ)
	$(CC) $(CFLAGS) -o $@ src/ttest.o $(LIBOBJ)

src/%.o: src/%.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -c -o $@ $<

test: ttest
	./ttest tests/*.s examples/*.s

clean:
	rm -f src/*.o $(PROGS) *.bin tests/*.bin examples/*.bin

.PHONY: all test clean
