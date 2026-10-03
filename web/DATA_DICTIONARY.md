# What is in these files

Two files, both produced by `tools/build_web_data.py` from the tables in
`build/`. The repack adds no information: every number here appears in
`build/` as well, in fuller form, and `build/` is the archive to check against.

**Every probability is an integer in ten-thousandths.** Divide by
10000. So `6712` means 0.6712, or 67.12%. Rounding is bounded by half a
ten-thousandth — a two-hundredth of a percentage point — which is finer than
any figure the page displays and finer than the simulation's own standard
error.

**Exact or estimated** is stated for every field below, and it is never left
to the reader to guess. Exact means complete enumeration: every possible board
counted, no sampling, no error bar. Estimated means simulation, and every
estimated field has its standard error alongside.

## `hands.json`

An object with `meta` and `hands`. `hands` is an array of 169 records, ordered
by strength against a random hand, strongest first.

| Field | Exact? | Meaning |
| --- | --- | --- |
| `hand` | — | The hand type: `AA`, `AKs` (same suit), `AKo` (different suits) |
| `family` | — | `pair`, `suited` or `offsuit` |
| `combos` | — | How many of the 1,326 concrete starting hands this type stands for: 6, 4 or 12. The weight to use when averaging over the 169 types |
| `rank` | exact | 1 to 169, by equity against a random hand |
| `percentile` | exact | The share of all 1,326 hands this one and everything stronger make up. What defines a "top X%" range |
| `equityVsRandom` | **exact** | Share of the pot against one opponent holding a random hand, counting a tie as half |
| `winVsRandom`, `tieVsRandom` | **exact** | The same showdown split into outright wins and ties. Losing is the remainder. Reported separately because a hand that wins less and ties more is a different hand |
| `equityVsOpponents` | estimated | Eight entries, for 1 to 8 opponents holding random hands |
| `equityVsOpponentsError` | — | The standard error of each of those. The true value is within about two of these, 95% of the time |
| `equityVsRanges` | **exact** | Seven entries, against an opponent playing the top 5%, 10%, 15%, 20%, 30%, 50%, 100% of hands |
| `potential` | **exact** | Nine entries, the chance of finishing in each hand category over all 2,118,760 boards. Exclusive and exhaustive: they sum to 1. **This is not the chance of winning** |
| `potentialUsingOwnCard` | **exact** | The same nine, requiring the best five cards to include at least one of your own. Barely differs from `potential`, because one of your cards riding along as a kicker satisfies the requirement — published to show that, not to be used |
| `improvement` | **exact** | Four entries summing to 1: your cards give a better category than the board makes, the same category with a bigger combination, the same combination with a better side card, or no improvement at all |
| `opponentHands.beatYou` | **exact** | Of the 1,225 hands an opponent can hold, how many your equity is below one half against |
| `opponentHands.splitWithYou` | **exact** | How many are worth exactly one half. These are the hands that mirror yours, such as AcKc against AdKd |
| `opponentHands.loseToYou` | **exact** | How many you are a favourite against. The three sum to 1,225 |
| `opponentHands.lowestEquity`, `medianEquity`, `highestEquity` | **exact** | The spread of your equity across those 1,225 hands |
| `opponentHands.bands` | **exact** | Eight counts, how many opponent hands fall in each equity band. Band upper edges are in `meta.equityBandUpperPercents`; the two either side of 50% are narrow on purpose, since that is where the beats-you line falls |
| `atLeastOneBeatsYou` | mixed | Eight entries: the chance at least one of n opponents holds a hand that beats yours. **Exact for 1 to 4 opponents, estimated for 5 to 8** — see `atLeastOneIsExact` |
| `atLeastOneIndependentApproximation` | — | What the common shortcut `1-(1-p)^n` gives. It is wrong, because the opponents' hands come out of one deck, and it almost always comes out too low. Shipped next to the right answer so the gap is visible |
| `atLeastOneIsExact` | — | Eight booleans saying which of the above are exact |

## `headsup-matrix.json`

The exact head-to-head matrix, loaded on demand because only the
hand-versus-hand calculator needs it.

| Field | Meaning |
| --- | --- |
| `hands` | The 169 labels, in the row and column order of the arrays below |
| `win` | 28,561 entries, row-major: `win[hero * 169 + opponent]` is how often the hero's hand wins outright |
| `tie` | The same, for ties |

Losing is `1 - win - tie`, and equity is `win + tie/2`. Every value is exact,
from complete enumeration of all 1,712,304 boards of every matchup.

## What is deliberately not here

**The all-in tables.** Every entry is two lines of arithmetic over
`equityVsRanges` and the stack depth, so the page computes them live:

    calling needs equity   (S - 1) / (2*S)
    calling is worth       2*S*q - (S - 1)
    shoving is worth       f*(S + 1) + (1 - f)*2*S*q - (S - 0.5)

in big blinds, heads-up blind versus blind with equal stacks of S. Sending a
precomputed table would be larger and worse: the reader moves the stack and
watches the required equity move.

**Anything per-street.** Flop and turn probabilities are not computed by this
engine at all. Everything here is preflop, with all five board cards dealt.
