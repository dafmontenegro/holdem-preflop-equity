# holdem-preflop-equity

Preflop probabilities for Texas Hold'em No Limit, computed from scratch and
reproducible with one command. Every figure published on
[montenegrodanielfelipe.com](https://montenegrodanielfelipe.com/) comes from
this engine, and no figure is published that the engine did not produce.

Two kinds of number live here, and they are never mixed:

- **Exact** results come from enumeration. Every possible case is visited and
  counted, so there is no error bar: the counts are the counts.
- **Estimated** results come from Monte Carlo simulation. Every one carries
  its own standard error, computed from its own sample.

The interactive trainer built on this data lives on the website; this
repository is the engine and the research behind it.

## What it computes today

| Table | Kind | What it says |
| --- | --- | --- |
| `potential_169_exact.csv` | Exact | How often each of the 169 starting hands finishes in each of the nine hand categories, over all 2,118,760 boards |
| `equity_169_vs_1to8_mc.csv` | Estimated | What each hand is worth against 1 to 8 opponents holding random hands, with wins, ties and losses counted separately |

**Potential is not the probability of winning.** Potential says how often a
hand makes a flush or a straight; equity says what share of the pot it
collects. They disagree often: 32o makes more straights than AKo and loses
badly to it, because what wins a pot is the better hand, not the rarer
category.

## Reproducing

Requires a C compiler and Python 3. No libraries are needed to generate the
data; `requirements-dev.txt` is only for cross-checking results against
outside evaluators and for rebuilding the reports.

```sh
make            # test, generate and verify: the whole chain
make test       # the evaluator's test suite
make data       # the tables, into build/
make verify     # compare build/ against exploration/ and across seeds
```

A full run from `make clean` takes about **2 minutes 45 seconds** on an Apple
M1, single-threaded. Monte Carlo runs record their trial count and seed in the
output, so any table can be reproduced exactly:

```sh
make data MONTE_CARLO_TRIALS=1000000
```

## How correctness is established

A poker evaluator is easy to write and hard to trust: a hand that is misread
once in ten thousand deals shifts every published number by a little and
breaks nothing visibly. So the engine is checked from four directions, and
none of the four is "the output looks about right".

**1. Named hands.** `src/test_eval7.c` spells out the cases where evaluators
go wrong, card by card, with the expected reading in the description: the
wheel at both ends of the ace's double role, kickers in every category, two
trips that must read as a full house, three pairs where the third pair plays
as the kicker, quads alongside trips, and hands where the player's own cards
do not play at all.

**2. Exhaustive enumeration of the hand space.** The nine category counts over
all 2,598,960 five-card hands, and over all 133,784,560 seven-card hands, must
match the published enumerations of poker hands. This cannot be passed by an
evaluator that is right about the common cases and wrong about the rare ones.

**3. The count of distinct hand values.** There are exactly **7,462** distinct
five-card hand values in poker. The engine maps all 2,598,960 five-card hands
and must land on exactly that many. This is the check that validates the
*ordering*, kickers included: the category counts above would still pass if
two-pair kickers were ignored, but merging hands that should rank differently
lowers the distinct count and fails here.

**4. Invariants on the generated tables.** Each generator validates its own
output and refuses to write a file if the check fails, so a broken build
cannot leave a plausible-looking CSV behind.

- *Exact potential:* every seven-card hand splits into "two private cards plus
  a board" in C(7,2) = 21 ways, so summing the counts over all 1,326 concrete
  starting hands counts every seven-card hand exactly 21 times. Dividing by 21
  must give the published seven-card counts, in all nine categories, with no
  remainder. The counts for each hand must also account for all 2,118,760
  boards.
- *Monte Carlo equity:* at a table where everyone holds a random hand nobody
  has an advantage, so each of the n+1 players is worth exactly 1/(n+1) of the
  pot. The combo-weighted average over the 169 types must come out at that
  value, at all eight table sizes, within sampling error. This constrains all
  169 hands jointly and catches a biased shuffle or a mis-weighted average,
  which per-hand plausibility would not.

**And then, from outside.** `make verify` compares the engine against the
earlier exploration phase under `exploration/`, which was written
independently: all **1,859** exact counts match cell for cell.

It also tests whether the reported error bars are honest, which is a separate
question from whether the equities are right. Two runs with different seeds are
independent samples of the same quantity, so their difference over the combined
error should behave like a standard normal. Over the 1,352 cells it does: mean
z of −0.02, standard deviation 1.03, 5.9% of cells beyond 1.96 and 1.3% beyond
2.58, against the 5% and 1% expected. The standard errors are not a formula
written next to the number; they describe the spread that is actually there.

## Layout

| Path | What it holds |
| --- | --- |
| `src/cards.h` | Card encoding, parsing and formatting |
| `src/eval7.h`, `src/eval7.c` | The evaluator: 5, 6 or 7 cards into one comparable score |
| `src/hands169.h` | The 169 starting hand types, their combo counts and representatives |
| `src/rng.h` | The random generator used by the Monte Carlo runs |
| `src/test_eval7.c` | The evaluator's test suite |
| `src/potential.c` | Exact potential of the 169 hands |
| `src/equity_mc.c` | Monte Carlo equity against 1 to 8 opponents |
| `tools/verify_reproduction.py` | Checks the output against `exploration/` and across seeds |
| `exploration/` | The first exploration phase, kept for provenance |
| `build/` | Generated tables. Not tracked: rebuild with `make data` |

Every header opens with a note on what it does and why it does it that way,
including the reasoning behind the bit tricks and the two places where a
careless random generator introduces bias. The engine is meant to be read,
not only run.

## Data dictionary

### `potential_169_exact.csv`

One row per starting hand type, 169 rows. All counts are exact, over the
C(50,5) = 2,118,760 boards that can follow that hand.

| Column | Meaning |
| --- | --- |
| `hand` | The hand type: `AA`, `AKs` (same suit), `AKo` (different suits) |
| `family` | `pair`, `suited` or `offsuit` |
| `combos` | How many of the 1,326 concrete starting hands this type stands for: 6, 4 or 12. Use it as the weight when averaging over the 169 types |
| `boards` | Always 2,118,760, the denominator for every count in the row |
| `high_card` … `straight_flush` | Boards on which the player's best five cards finish in that category. The nine are mutually exclusive and sum to `boards` |
| `improves_board` | Boards where the player's cards raise the category the board makes on its own. **Approximate:** it misses improvements inside one category, such as holding a higher flush than the board, so it understates how often the player's cards matter |

### `equity_169_vs_1to8_mc.csv`

One row per hand type and opponent count, 169 × 8 = 1,352 rows. Estimated.

| Column | Meaning |
| --- | --- |
| `hand`, `family`, `combos` | As above |
| `opponents` | How many opponents, 1 to 8, each holding a uniformly random hand |
| `trials` | Showdowns simulated for this row |
| `wins`, `ties`, `losses` | Trials where the hand was strictly best, equally best, or beaten. Reported separately because they are different facts, and because a tie pays a fraction, not nothing |
| `equity` | Average share of the pot, counting a tie among k players as 1/k. That fraction is not an approximation: a tied pot is split k ways |
| `standard_error` | Standard error of `equity`, from this row's own sample variance. The true value lies within about two of these of the estimate, 95% of the time |

The two header lines beginning with `#` record the seed.

## What this model assumes, and does not

Opponents are dealt **uniformly random hands** and every hand goes to the
river. That is the right model for one question — what is this hand worth
against n unknown hands — and the wrong model for a real table, where
opponents fold their worst hands and continue with their better ones. Against
players who only call with strong hands, true equity is **lower** than these
figures.

Cards are dealt without replacement from one deck, so nothing assumes
independence between hands: the opponents' cards, the board and the player's
own cards all come out of the same 52.

Not modelled at all: betting, position, stack sizes, opponent behaviour, and
the strength of a hand within its category. A nine-high straight and an
ace-high straight are the same category here, and the equity table is the
place where that difference is actually priced.

## Still to come

- The exact head-to-head 169 × 169 matrix, by full board enumeration with suit
  isomorphism, and the exact equity against a random hand derived from it.
- The distribution of how many opponents hold a hand that beats yours, without
  assuming independence between their hands.
- A hand ranking and equity against opponents playing the top X% of hands.
- The preflop all-in decision: expected value with dead money, effective
  stacks, and the probability that everyone folds as a parameter rather than a
  guess.
- A refined potential that requires at least one of the player's own cards to
  play, which is what `improves_board` only approximates today.

### A known cost

The exact potential takes 38 seconds here, against 11 for the exploration
phase's program, because this evaluator resolves full kickers on every board
where the old one only needed a category. That is the right trade for a single
evaluator that is exhaustively validated, and it is paid once. The 169 × 169
matrix is a different scale of work and will need the suit isomorphism and
several threads; the design note will go with it.
