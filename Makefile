# Makefile — rebuilds everything the project publishes, from nothing.
#
# `make` runs the whole chain in the order that matters: prove the evaluator
# first, then generate data with it, then check that data against the
# exploration phase and against itself. Each step fails loudly, and the
# generators refuse to write a file whose own validation did not pass, so a
# broken build cannot leave a plausible-looking CSV behind.
#
#   make           test, generate and verify: the whole thing (about 11 minutes)
#   make test      the evaluator's test suite, including the exhaustive checks
#   make data      the published tables, into build/
#   make verify    compare build/ against exploration/ and across seeds
#   make clean     remove binaries and generated data
#
# Monte Carlo runs take a trial count and a seed, both recorded in the output,
# so any table can be reproduced exactly:
#
#   make data MONTE_CARLO_TRIALS=1000000
#   make data THREADS=4

CC      ?= cc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra
LDLIBS  := -lm
PYTHON  ?= python3

SRC   := src
BIN   := bin
BUILD := build

# Trials per cell for the Monte Carlo equity table. At 100,000 the standard
# error of a cell is at most about 0.16 percentage points.
MONTE_CARLO_TRIALS ?= 100000

# Two seeds, because one run cannot check its own error bars. The second run
# is an independent sample of the same quantities, and `make verify` compares
# them cell by cell.
SEED_PRIMARY   ?= 1
SEED_SECONDARY ?= 2

HEADERS := $(SRC)/cards.h $(SRC)/eval7.h $(SRC)/hands169.h $(SRC)/rng.h \
           $(SRC)/suits.h

DATA := $(BUILD)/potential_169_exact.csv \
        $(BUILD)/equity_169_vs_1to8_mc.csv \
        $(BUILD)/equity_169_vs_1to8_mc_seed2.csv \
        $(BUILD)/headsup_169x169_exact.csv

# Worker threads for the head-to-head matrix, the only step that is
# parallelised and the only one long enough to need it.
THREADS ?= 8

.PHONY: all test data verify clean
.DEFAULT_GOAL := all

all: test data verify

# ---------------------------------------------------------------------------
# Binaries
# ---------------------------------------------------------------------------

$(BIN) $(BUILD):
	mkdir -p $@

$(BIN)/test_eval7: $(SRC)/test_eval7.c $(SRC)/eval7.c $(HEADERS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $(SRC)/test_eval7.c $(SRC)/eval7.c

$(BIN)/potential: $(SRC)/potential.c $(SRC)/eval7.c $(HEADERS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $(SRC)/potential.c $(SRC)/eval7.c

$(BIN)/equity_mc: $(SRC)/equity_mc.c $(SRC)/eval7.c $(HEADERS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $(SRC)/equity_mc.c $(SRC)/eval7.c $(LDLIBS)

$(BIN)/headsup: $(SRC)/headsup.c $(SRC)/eval7.c $(HEADERS) | $(BIN)
	$(CC) $(CFLAGS) -pthread -o $@ $(SRC)/headsup.c $(SRC)/eval7.c

# ---------------------------------------------------------------------------
# Steps
# ---------------------------------------------------------------------------

# The evaluator is the foundation: every number in the project is a count of
# its verdicts, so it is tested before anything is generated. Takes about 10
# seconds, nearly all of it enumerating all 133,784,560 seven-card hands.
test: $(BIN)/test_eval7
	./$(BIN)/test_eval7

data: $(DATA)

# Exact: enumerates all 2,118,760 boards for each of the 169 hands and
# validates the result against the published seven-card hand counts.
$(BUILD)/potential_169_exact.csv: $(BIN)/potential | $(BUILD)
	./$(BIN)/potential $@

# Estimated: two independent runs, each validating that a random hand is worth
# exactly its fair share of the pot before writing anything.
$(BUILD)/equity_169_vs_1to8_mc.csv: $(BIN)/equity_mc | $(BUILD)
	./$(BIN)/equity_mc $(MONTE_CARLO_TRIALS) $(SEED_PRIMARY) $@

$(BUILD)/equity_169_vs_1to8_mc_seed2.csv: $(BIN)/equity_mc | $(BUILD)
	./$(BIN)/equity_mc $(MONTE_CARLO_TRIALS) $(SEED_SECONDARY) $@

# Exact: enumerates every board of all 47,086 matchups left after suit
# isomorphism and mirroring, and checks five invariants in integer arithmetic
# before writing. The longest step by far, about 8 minutes on 8 threads. The
# second file is one row of the matrix aggregated, so the two are written
# together and cannot disagree.
$(BUILD)/headsup_169x169_exact.csv $(BUILD)/equity_169_vs_random_exact.csv: $(BIN)/headsup | $(BUILD)
	THREADS=$(THREADS) ./$(BIN)/headsup \
	    $(BUILD)/headsup_169x169_exact.csv \
	    $(BUILD)/equity_169_vs_random_exact.csv

verify: $(DATA)
	$(PYTHON) tools/verify_reproduction.py

clean:
	rm -rf $(BIN) $(BUILD)
