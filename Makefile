# Makefile — rebuilds everything the project publishes, from nothing.
#
# `make` runs the whole chain in the order that matters: prove the evaluator
# first, then generate data with it, then check that data against the
# exploration phase and against itself. Each step fails loudly, and the
# generators refuse to write a file whose own validation did not pass, so a
# broken build cannot leave a plausible-looking CSV behind.
#
#   make           test, generate and verify: the whole thing (about 21 minutes)
#   make test      the evaluator's test suite, including the exhaustive checks
#   make data      the published tables, into build/ (enumerated and derived)
#   make verify    compare build/ against exploration/ and across seeds
#   make crosscheck  check individual matchups against treys, an outside
#                    evaluator. Needs requirements-dev.txt and takes about
#                    3 minutes, so it is not part of `make`.
#   make convergence  measure the simulation's error against the exact
#                     answers at three trial counts. About 2 minutes.
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

# Trials per cell for the Monte Carlo equity table. At 400,000 the standard
# error of a cell is at most about 0.08 percentage points, and the measured
# root-mean-square error against the exact one-opponent equities is 0.0757.
# `make convergence` is what established that those two agree.
MONTE_CARLO_TRIALS ?= 400000

# Two seeds, because one run cannot check its own error bars. The second run
# is an independent sample of the same quantities, and `make verify` compares
# them cell by cell.
# Simulation trials per cell for 5 to 8 opponents, where the exact method
# runs out. At 200,000 the standard error of a probability is at most 0.11
# percentage points.
BEATEN_TRIALS ?= 200000

SEED_PRIMARY   ?= 1
SEED_SECONDARY ?= 2

HEADERS := $(SRC)/cards.h $(SRC)/eval7.h $(SRC)/hands169.h $(SRC)/rng.h \
           $(SRC)/suits.h

DATA := $(BUILD)/potential_169_exact.csv \
        $(BUILD)/equity_169_vs_1to8_mc.csv \
        $(BUILD)/equity_169_vs_1to8_mc_seed2.csv \
        $(BUILD)/headsup_169x169_exact.csv \
        $(BUILD)/hand_ranking_exact.csv \
        $(BUILD)/beaten_at_least_one.csv \
        $(BUILD)/allin_call_exact.csv \
        $(BUILD)/potential_refined_169_exact.csv

# Worker threads for the head-to-head matrix, the only step that is
# parallelised and the only one long enough to need it.
THREADS ?= 8

.PHONY: all test data verify crosscheck convergence clean
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

$(BIN)/refined_potential: $(SRC)/refined_potential.c $(SRC)/eval7.c $(HEADERS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $(SRC)/refined_potential.c $(SRC)/eval7.c

$(BIN)/beaten: $(SRC)/beaten.c $(SRC)/eval7.c $(HEADERS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $(SRC)/beaten.c $(SRC)/eval7.c $(LDLIBS)

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

# Exact: the same enumeration as the potential above, but also asking what the
# hand would be if it had to use one of the player's own cards, and sorting
# every board by what kind of improvement the player's cards made.
$(BUILD)/potential_refined_169_exact.csv: $(BIN)/refined_potential | $(BUILD)
	./$(BIN)/refined_potential $@

# Estimated: two independent runs, each validating that a random hand is worth
# exactly its fair share of the pot before writing anything.
$(BUILD)/equity_169_vs_1to8_mc.csv: $(BIN)/equity_mc | $(BUILD)
	./$(BIN)/equity_mc $(MONTE_CARLO_TRIALS) $(SEED_PRIMARY) $@

$(BUILD)/equity_169_vs_1to8_mc_seed2.csv: $(BIN)/equity_mc | $(BUILD)
	./$(BIN)/equity_mc $(MONTE_CARLO_TRIALS) $(SEED_SECONDARY) $@

# Exact: enumerates every board of all 93,769 matchups left after suit
# isomorphism, and checks six invariants in integer arithmetic before writing.
# The longest step by far, about 14 minutes on 8 threads. Both halves of the
# matrix are enumerated rather than one being mirrored from the other, which
# costs 7 of those minutes and turns the mirror relation into a check. The
# three files are written together from one run and cannot disagree.
$(BUILD)/headsup_169x169_exact.csv $(BUILD)/equity_169_vs_random_exact.csv \
$(BUILD)/headsup_169_vs_1225_exact.csv: $(BIN)/headsup | $(BUILD)
	THREADS=$(THREADS) ./$(BIN)/headsup \
	    $(BUILD)/headsup_169x169_exact.csv \
	    $(BUILD)/equity_169_vs_random_exact.csv \
	    $(BUILD)/headsup_169_vs_1225_exact.csv

# Exact for 1 to 4 opponents, estimated for 5 to 8. Checks the
# inclusion-exclusion against brute force before writing.
$(BUILD)/equity_landscape_169_exact.csv $(BUILD)/beaten_by_k.csv \
$(BUILD)/beaten_at_least_one.csv: $(BIN)/beaten $(BUILD)/headsup_169_vs_1225_exact.csv
	./$(BIN)/beaten \
	    $(BUILD)/headsup_169_vs_1225_exact.csv \
	    $(BUILD)/equity_landscape_169_exact.csv \
	    $(BUILD)/beaten_by_k.csv \
	    $(BUILD)/beaten_at_least_one.csv \
	    $(BEATEN_TRIALS) $(SEED_PRIMARY)

# Derived, not enumerated: the hand ranking and the top X% ranges are sums of
# counts already in the exact matrix, so they are exact too. Done in exact
# rational arithmetic, and the script validates itself before writing.
$(BUILD)/hand_ranking_exact.csv $(BUILD)/equity_169_vs_ranges_exact.csv: \
        tools/derive_ranges.py $(BUILD)/headsup_169x169_exact.csv
	$(PYTHON) tools/derive_ranges.py

# Derived: the all-in decision. Exact equities from the matrix, combined with
# pot odds. Prints its verdict on the "call with JJ+ and AK" rule.
$(BUILD)/allin_call_exact.csv $(BUILD)/allin_shove_exact.csv: \
        tools/derive_allin.py $(BUILD)/headsup_169x169_exact.csv \
        $(BUILD)/hand_ranking_exact.csv
	$(PYTHON) tools/derive_allin.py

verify: $(DATA)
	$(PYTHON) tools/verify_reproduction.py

# The one check that compares against code written by other people. Kept out
# of `make` because it needs treys installed and enumerates 1.7 million boards
# per matchup in Python.
crosscheck: $(BUILD)/headsup_169_vs_1225_exact.csv
	$(PYTHON) tools/crosscheck_treys.py

# Runs the simulation at three trial counts and measures its error against the
# exact one-opponent equities, which should fall as one over the square root of
# the trials. Kept out of `make` because it re-runs the simulation three times.
convergence: $(BIN)/equity_mc $(BUILD)/equity_169_vs_random_exact.csv
	$(PYTHON) tools/convergence_test.py

clean:
	rm -rf $(BIN) $(BUILD)
