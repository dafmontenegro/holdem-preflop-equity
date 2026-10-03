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
| `potential_refined_169_exact.csv` | Exact | The same, plus what the hand would be if it had to use one of your own cards, and what kind of improvement your cards actually made |
| `headsup_169x169_exact.csv` | Exact | Wins, ties and losses for every ordered pair of starting hands, over every board. All 28,561 cells |
| `equity_169_vs_random_exact.csv` | Exact | What each hand is worth against one opponent holding a random hand, aggregated from the matrix |
| `hand_ranking_exact.csv` | Exact | The 169 hands ordered by equity against a random hand, with the cumulative share that defines the top X% ranges |
| `equity_169_vs_ranges_exact.csv` | Exact | What each hand is worth against an opponent playing the top 5, 10, 15, 20, 30, 50 or 100% of hands |
| `headsup_169_vs_1225_exact.csv` | Exact | Every distinct matchup: each hand against each of the 1,225 hands an opponent can hold |
| `equity_landscape_169_exact.csv` | Exact | The shape of that distribution per hand: how many opponent hands beat it, split with it, lose to it, and the spread in bands |
| `beaten_by_k.csv` | Both | How many of n opponents hold a beating hand. Exact for 1 to 4 opponents, estimated for 5 to 8 |
| `beaten_at_least_one.csv` | Both | The chance at least one of n opponents beats you, next to the independence shortcut it replaces |
| `allin_call_exact.csv` | Exact | Whether calling an all-in makes money, by hand, the shover's range and the stack depth |
| `allin_shove_exact.csv` | Exact | What shoving is worth, by hand, the caller's range and the stack depth, with the fold probability counted from the range |
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
make             # test, generate, repack and verify: the whole chain
make test        # the evaluator's test suite
make data        # the tables, into build/
make web         # repack them into the files the website loads, into web/
make verify      # compare build/ against exploration/ and across seeds
make crosscheck  # check matchups against treys (needs requirements-dev.txt)
make convergence # measure the simulation's error at three trial counts
```

A full run from `make clean` takes about **18 minutes** on an Apple M1, of
which 16 are the exact head-to-head matrix, the only step that is
parallelised. Monte Carlo runs record their trial count and seed in the
output, so any table can be reproduced exactly:

```sh
make data MONTE_CARLO_TRIALS=1000000
make data THREADS=4
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
- *The exact matrix:* six invariants, all in integer arithmetic, because
  nothing in the matrix is estimated and so there is no tolerance to choose.
  A pair of types must have the same number of deals read in either
  direction; every hand must face all 1,225 opponent hands; wins, ties and
  losses must account for every board of every deal; the matrix must be its
  own mirror; a hand against its own type must win and lose equally often, so
  it is worth exactly half the pot; and the whole matrix must average to
  exactly one half, since neither of two random hands has an advantage. The
  last one is checked as an equality between two integers, both
  2,781,381,002,400.

  The mirror check is worth a word, because it costs half the running time.
  A against B gives B against A by swapping wins and losses, so half the
  matrix could be copied rather than computed, in four minutes instead of
  fourteen. Both halves are enumerated instead, and the mirror relation is
  then a **check** rather than an assumption: the two halves are genuinely
  different calculations, since A's representative against concrete hands of
  type B uses different cards from B's representative against concrete hands
  of type A, so their agreement in all 14,196 mirrored pairs tests the suit
  isomorphism, the orbit weighting and the scaling at once. Copying would have
  tested none of them.
- *How many opponents beat you:* the inclusion-exclusion is checked against
  brute force, which shares none of its reasoning — every deal walked and
  classified one at a time. They agree exactly for two opponents on all 169
  hands, and for three opponents on four sampled hands at 300 million deals
  each.

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

Since the matrix exists, the simulation can be held against the truth rather
than against another simulation. Measuring the one-opponent estimates against
the exact values, in units of each estimate's own standard error, gives a mean
z of −0.008 and a standard deviation of 1.02: the simulation is unbiased and
its error bars are the right size.

And finally against figures nobody here computed. The equity of a hand against
a random opponent has been published for decades, and the engine reproduces
every one of the ten spot-checked hands to the tenth of a point they are
quoted to — AA at 85.204%, AKo at 65.320%, 72o at 34.584% — with a widest gap
of 0.045 percentage points. That ties the whole chain, evaluator through suit
isomorphism through weighting, to other people's code.

**Does more simulation actually help?** `make convergence` runs the simulation
at 25,000, 100,000 and 400,000 trials and measures the root-mean-square error
of the one-opponent column against the exact values from the matrix. Sampling
error should fall as one over the square root of the trials, so quadrupling
them should halve it. It does: 0.3031, 0.1662 and 0.0757 percentage points,
ratios of 1.82 and 2.20 against 2.00 predicted. At each trial count the error
the runs actually have is within 10% of the error they report. A simulation
that had stopped improving — a biased generator, a shuffle not reaching every
deal, an accumulator losing precision — would show a ratio drifting towards 1
while every individual run still looked fine.

**Against code written by other people.** `make crosscheck` enumerates all
1,712,304 boards of seven chosen matchups with [treys](https://pypi.org/project/treys/),
an independent evaluator, and compares **wins, ties and losses as exact
integers** — not the equity, which could agree by luck while the three counts
are wrong. The matchups are chosen for what is most likely to break: a
dominated hand where kickers decide nearly every board, the same two hand
types in two suit arrangements, matchups turning on flushes and on the wheel,
and a pair against two overcards where ties are common enough to catch one
miscounted as a win. All seven agree on every count.

That check is also what caught my own wrong assumption. AKo against AA comes
out at 6.53% or 7.43% depending on the suits, and I expected about 12% from
memory. The engine was right and the memory was wrong: pairing the king does
not help, because a pair of kings loses to the pair of aces, so the hand is
drawing to a straight, a flush or trips. Had I trusted the recollection over
the check, I would have gone looking for a bug that was not there.

## Layout

| Path | What it holds |
| --- | --- |
| `src/cards.h` | Card encoding, parsing and formatting |
| `src/eval7.h`, `src/eval7.c` | The evaluator: 5, 6 or 7 cards into one comparable score |
| `src/hands169.h` | The 169 starting hand types, their combo counts and representatives |
| `src/rng.h` | The random generator used by the Monte Carlo runs |
| `src/test_eval7.c` | The evaluator's test suite |
| `src/suits.h` | Renaming suits: the symmetry that makes the exact matrix feasible |
| `src/potential.c` | Exact potential of the 169 hands |
| `src/headsup.c` | The exact 169 x 169 head-to-head matrix, and every distinct matchup |
| `src/refined_potential.c` | The potential your own two cards are responsible for |
| `src/beaten.c` | How many opponents hold a hand that beats yours |
| `tools/derive_ranges.py` | The hand ranking and the top X% ranges, derived from the matrix in exact rational arithmetic |
| `tools/derive_allin.py` | The all-in decision, and the verdict on the "JJ+ and AK" rule |
| `src/equity_mc.c` | Monte Carlo equity against 1 to 8 opponents |
| `tools/verify_reproduction.py` | Checks the output against `exploration/` and across seeds |
| `tools/crosscheck_treys.py` | Checks individual matchups against treys, an outside evaluator |
| `tools/convergence_test.py` | Measures the simulation's error against the exact answers at three trial counts |
| `exploration/` | The first exploration phase, kept for provenance |
| `tools/build_web_data.py` | Repacks the tables into the files the website loads |
| `build/` | Generated tables, the archive to check everything against. Not tracked: rebuild with `make data` |
| `web/` | The same numbers repacked for the site, with a dictionary documenting every field. Tracked, because it is the exact data the site was served |

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
| `improves_board` | Boards where the player's cards raise the category the board makes on its own. **Superseded:** it misses improvements inside one category, such as holding a higher flush than the board. `potential_refined_169_exact.csv` replaces it with a four-way split that does not. Kept because the exploration phase published it |

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

### `headsup_169x169_exact.csv`

One row per ordered pair of hand types, 28,561 rows, 1.6 MB. Exact.

| Column | Meaning |
| --- | --- |
| `hero`, `opponent` | The two hand types. Both orders are present, so the file can be read as a square matrix |
| `deals` | Ordered pairs of concrete starting hands with these two types that share no card. Not the same in both directions of a pair until it is multiplied out, which is why it is recorded |
| `boards` | Boards counted for this cell: `deals` x 1,712,304 |
| `wins`, `ties`, `losses` | Boards on which the hero's hand was strictly better, equal, or beaten |
| `equity` | `(2 x wins + ties) / (2 x boards)`. A head-to-head tie is always split two ways, so a tie is worth half a board |

### `equity_169_vs_random_exact.csv`

One row per hand type, 169 rows. Exact: this is a row of the matrix
aggregated, not a separate calculation, so the two files cannot disagree.

| Column | Meaning |
| --- | --- |
| `hand`, `family`, `combos` | As above |
| `boards` | Boards behind the row: every board of every deal against all 1,225 opponent hands |
| `wins`, `ties`, `losses` | Counted over those boards |
| `equity` | Exact equity against one opponent holding a uniformly random hand |
| `win_rate`, `tie_rate` | Wins and ties as fractions of `boards`. Reported alongside equity because a hand that wins less but ties more is a different hand, and the single equity figure hides that: 98s and 22 are worth almost the same (50.80% against 50.33%) but 98s ties twice as often |

### `headsup_169_vs_1225_exact.csv`

One row per distinct matchup, 93,769 rows, 5 MB. Exact.

Behind "AKs is worth 67% against a random hand" are 1,225 specific opponent
hands, and the average hides the shape of what it averages. This file is the
shape: every hand against every hand an opponent can hold.

| Column | Meaning |
| --- | --- |
| `hero` | The hand type, computed from its representative cards |
| `opponent_cards` | The specific two cards, e.g. `AhAs` |
| `opponent_hand` | Their hand type |
| `stands_for` | How many of the 1,225 opponent hands this row speaks for. Opponent hands that renaming the unused suits maps onto each other have identical equity against the hero, so they share a row. Summing this across a hero's rows gives exactly 1,225 |
| `boards`, `wins`, `ties`, `losses`, `equity` | Over the 1,712,304 boards of one matchup. Unscaled, unlike the matrix file, which is the scaled aggregate of these |

### `equity_landscape_169_exact.csv`, `beaten_by_k.csv` and `beaten_at_least_one.csv`

"What are the chances somebody has a better hand than me?" has no answer
preflop until it is given a definition, because hand strength is only decided
once the board is out. The definition used here:

> An opponent hand **beats** yours when your exact equity against that
> specific hand is below one half.

That is a statement about the long run against one hand, not a prediction
about this pot, and it throws away *how far* below one half: a hand you beat
49.9% of the time counts the same as one you beat 20% of the time. Which is
why `equity_landscape_169_exact.csv` reports the whole distribution in bands
as well as the classification. The boundary is handled exactly: equity is the
rational number `(2*wins + ties) / (2*boards)`, so "below one half" is the
integer test `2*wins + ties < boards`, and hands landing exactly on one half
are counted separately rather than pushed to one side. They are real — they
are the hands that mirror yours, such as AcKc against AdKd.

The multiple-opponent version is where it gets hard, and that is the point of
these files. The familiar shortcut `1 - (1-p)^n` assumes the opponents' hands
are independent, and they are not: they come out of one deck. The right answer
needs the number of ways to deal n card-disjoint hands with exactly k of them
beating you, which is counting matchings of size n in a graph on the 50
remaining cards — a known hard problem, with on the order of 10^17 of them at
n = 8. So it is **exact for 1 to 4 opponents** by inclusion-exclusion, and
**estimated with standard errors for 5 to 8**. That boundary is where the
method runs out, not where patience does: reaching five would mean enumerating
disjoint sets of four beating hands, about 90 billion steps per starting hand.

`beaten_at_least_one.csv` puts the right answer next to the shortcut and
records the gap in percentage points, so the cost of assuming independence is
a column rather than a claim.

### `hand_ranking_exact.csv` and `equity_169_vs_ranges_exact.csv`

The ranking orders the 169 types by exact equity against a random hand, and
carries the cumulative share of the 1,326 concrete hands, which is what
defines a "top X%" range. The range table then gives each hand's exact equity
against an opponent playing the top 5, 10, 15, 20, 30, 50 or 100% of hands.

Both are sums of counts already in the matrix, computed in exact rational
arithmetic, so both are exact. Equity against the top 100% must *equal*
equity against a random hand, and the weighted average over all 1,326 hands
must be exactly one half; the script checks both as equalities and refuses to
write if either fails.

**Two things about this ranking are choices, not facts.** Ordering by equity
against a random hand is the only ordering here that is computed rather than
asserted, which is why it was used, but it is not how strong players rank
hands: it has no notion of position, stack depth or what happens after the
flop, and it ranks 88 and 77 inside the top 5% while leaving every suited
connector far down the list. And ranges are built from whole hand types, never
split, so a range asked for 5% actually covers 5.43%; the table records what
each one really covers. Splitting a type would mean claiming an opponent plays
AKo from three suit combinations and folds the fourth.

### `allin_call_exact.csv` and `allin_shove_exact.csv`

The situation modelled is heads-up, blind versus blind, both players holding
the same stack of S big blinds — the canonical push-or-fold spot, chosen
because it is the one that is exactly solvable and the one the "JJ+ and AK"
rule is quoted about. Money is counted in **expected final stacks**, which
sidesteps every argument about whether a posted blind is still yours: each row
reports the difference between acting and folding, in big blinds.

**Calling.** You are the big blind; the small blind is all-in for S, so
calling costs S − 1 more and the pot becomes 2S.

    EV(call) − EV(fold) = 2·S·q − (S − 1),    so you need  q* = (S − 1) / (2·S)

That formula is why "you need 50% to call an all-in" is wrong. It is 40% at 5
big blinds, 45% at 10, 47.5% at 20 and 49.5% at 100. The blinds are already in
the pot and are not yours any more, and the shorter the stacks the larger a
share of the pot they are.

**Shoving.** You are the small blind, all-in for S. They fold with probability
f, and otherwise call.

    EV(shove) − EV(fold) = f·(S + 1) + (1 − f)·2·S·q − (S − 0.5)

Note which q that is: equity **given that they called**, so measured against
their calling range and not against a random hand. A shove earns from two
different places, the folds and the called pots, and merging them is the usual
way the calculation goes wrong.

**f is counted, not guessed.** Once a calling range is named, the chance they
fold is the chance their cards are not in it, and the deal counts already in
the matrix give it exactly — including the blocker effect: holding AA removes
five of the six ways they can hold AA, so a tight range is slightly less
likely for them and f slightly larger. Holding AA they fold a top 5% range
95.5102% of the time; holding 72o, 94.3673%.

Each row also carries the **breakeven fold probability**. Where it reads
`always`, the shove makes money however often they fold — the equity when
called already carries it, and you would rather be called than folded to. That
is not a curiosity: with aces at 5 big blinds against a 20% caller, being
folded to is worth +1.500 bb and being called every time is worth +4.068 bb.
Wanting a fold is a property of weak hands, not of shoving.

**The verdict on "against an all-in, call only with JJ+ and AK".** Refuted in
both directions, and `make data` prints the whole argument with the numbers.
Against a tight shove it is too loose: facing a top 5% range, AKo is a
profitable call only at 5 big blinds and AKs only up to 20, because the hands
that beat AK against that range are precisely the hands such a player shoves.
Against anything wider it is far too tight: facing a top 20% shove, 26 hands
are profitable calls at 10 big blinds and 19 at 20, against the rule's six.
A fixed list of hands cannot be right, because it names neither of the two
things that decide the answer — the shover's range, which sets your equity,
and the stack depth, which sets the equity you need. What survives is the
rule's shape: pairs and ace-broadways do dominate every calling range here.

**What the all-in model does not do.** No position beyond blind versus blind,
no unequal stacks and therefore no side pots, no antes, no third player, and
no tournament prize structure, so a chip is worth a chip and none of this is
ICM. And the load-bearing one: **the opponent does not react.** Their range is
a fixed input. That is most visible in the shove table, where at 10 big blinds
against an opponent calling the top 20%, all 169 hands shove profitably, 32o
included. The number is correct for the question asked and is not advice: an
opponent who noticed would call much wider, and most of those shoves would
stop working. Finding ranges that are stable against each other is a Nash
equilibrium problem for the push-or-fold game, and this file does not solve
it. It answers what a play is worth against *that* opponent, which is a more
modest question.

### `potential_refined_169_exact.csv`

Plain potential flatters weak hands. 72o reaches two pair or better on 34% of
boards, which sounds playable and is not, because most of those are two pair
sitting on the board, which everyone else at the table has too. This file
measures what your own two cards are responsible for, two different ways, and
the first of the two mostly fails — which is itself the finding.

**What the hand would be if it had to use one of your cards.** The columns
`hole_*` give the category of the best five cards that includes at least one
card only you hold. This barely differs from plain potential: 72o's two pair
or better goes from 34.24% to 34.04%, and the largest gap across all 169 hands
is 0.41 points. The reason is that "uses one of your cards" is satisfied by a
card riding along as a kicker — the board's two pair plus your seven as the
fifth card uses your seven. So the suspicion that a weak hand's potential is
mostly the board's turns out to be **wrong as stated**, and these columns are
published to show that rather than to be used.

**What kind of improvement your cards made.** This is the measure that works,
and it is the one the exploration phase asked for: a metric that notices
improvements inside a category, such as holding a king-high flush where the
board alone makes a nine-high flush. Every board sorts into exactly one of
four buckets, read straight off the score layout:

| Column | Meaning |
| --- | --- |
| `raises_category` | Your cards give you a better category than the board makes on its own |
| `raises_defining_rank` | Same category, but your combination is bigger: a higher pair, a higher flush, a higher straight |
| `improves_kicker_only` | Same category and the same defining rank, but a better side card. Your cards decide a pot between two players who both have the board's hand, and nothing more |
| `no_improvement` | The board's hand is your hand |

The split matters because lumping the middle two together is misleading in
both directions, and the sharpest result in this file depends on it:

> **AA and 22 raise the board's category exactly equally often, 94.72% of the
> time. But AA goes on to beat the board's own combination 4.61% of the time,
> and 22 does so 0.00% of the time.**

A deuce is never a bigger pair than the board's pair and never a useful
kicker, so a pocket pair of deuces either makes the category or contributes
nothing at all — 5.28% of boards it does nothing, against 0.50% for aces. That
is the measured version of the exploration phase's erratum that not all pocket
pairs are alike, and it is sharper than the straight/flush argument that
erratum was based on.

Elsewhere the kicker column is where most hands live: T2o improves on the
board by a kicker alone on 45.34% of boards, which is most of what it does.
And 32o fails to improve on the board at all on **47.33%** of boards, against
9.13% for 72o, because a three and a deuce lose to the board's own side cards
while a seven often plays as one.

## What the website is given

GitHub Pages serves static files, so the site cannot run the engine. `make
web` hands it a repack instead: two JSON files and the dictionary documenting
every field of them. The repack adds no information — if a number appears
there and not in `build/`, that is a bug — and it checks for exactly that
before writing, re-testing the invariants that survived the reshaping: the
nine categories still sum to one, every hand still classifies all 1,225
opponent hands, the matrix is still its own mirror, and every hand's matrix
row still averages back to its equity against a random hand.

Three decisions are worth naming, because they are the only places the site's
numbers can differ from the engine's.

**Rounding.** Probabilities become integers in ten-thousandths, so 0.671234 is
stored as 6712 — a hundredth of a percentage point. That is finer than
anything the page displays and more than ten times finer than the simulation's
own standard error of 0.0757. The packer measures the worst rounding it
actually introduced and refuses to write if it exceeds half a ten-thousandth,
which is the arithmetic limit of rounding to that precision. An exact figure
stays exact in the sense that matters: it is rounded, not estimated, and by a
bounded and reported amount.

**What the browser computes rather than downloads.** The all-in tables are not
shipped. Every entry is two lines of arithmetic over the equity against a
range and the stack depth, both already being sent, so the page computes them
live from the formulas. Smaller, and better to read: the stack becomes
something the reader moves while watching the required equity move, instead of
a column in a table. Nothing else is recomputed in the browser, because
everything else is a count over billions of enumerated boards.

**Two files, loaded at different times.** `hands.json` (135 KB) holds
everything that is one number per hand — what the grid, the hand sheet and the
trainer all need — and loads immediately. `headsup-matrix.json` (240 KB) holds
the 28,561 cells only the hand-versus-hand calculator needs, so it loads on
demand and nobody who never opens that calculator pays for it. Win and tie
rates are both sent and losing is the remainder: not to save space, but so the
page can show the three separately, which this project insists on everywhere.

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

