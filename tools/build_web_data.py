"""Packs the engine's tables into the files the website loads.

WHY THIS IS A SEPARATE STEP
---------------------------
The CSV files under build/ are the engine's output and the archive: wide,
fully named, exact integer counts, meant to be read and checked. They are the
wrong shape for a web page, which needs to load on a phone over a slow
connection and wants one compact file per thing it draws.

So this step repacks them. It adds no information and computes no new result;
if a number appears here that is not in build/, that is a bug. What it does
do is make three decisions worth stating, because they are the only places
where the website's numbers can differ from the engine's.

DECISION 1: ROUNDING, AND HOW MUCH IT COSTS
-------------------------------------------
Probabilities are stored as integers in ten-thousandths, so 0.671234 becomes
6712. That is four decimal places, or a hundredth of a percentage point, which
is finer than any figure the page displays and far finer than the simulation's
own standard error of about 0.0008. The script measures the worst rounding
error it actually introduced and refuses to write if it exceeds half a
ten-thousandth, which is the arithmetic limit.

Exact figures stay exact in the sense that matters: they are rounded, not
estimated, and the rounding is bounded and reported. Where the page needs to
say "exact", it still can, because the quantity is exact and only its printed
form is truncated.

DECISION 2: WHAT IS SENT AND WHAT IS COMPUTED IN THE BROWSER
------------------------------------------------------------
The all-in tables are not shipped. They are 169 hands x 7 ranges x 8 stack
depths x two decisions, and every entry is two lines of arithmetic over
numbers that are already being sent: the equity against the range and the
stack depth. So the page computes them live from the same formulas the
engine's documentation states. That keeps the download small and makes the
page better: the reader moves the stack slider and watches the required equity
move, rather than reading a table of precomputed answers.

The same reasoning does not apply to anything else here. Every other figure is
a count over billions of enumerated boards and could not be recomputed in a
browser at any speed.

DECISION 3: TWO FILES, LOADED AT DIFFERENT TIMES
------------------------------------------------
`hands.json` holds everything that is one number per hand, which is what the
grid, the hand sheet and the trainer need. It is small and loads immediately.

`headsup-matrix.json` holds the 28,561 cells of the head-to-head matrix, which
only the hand-versus-hand calculator needs. It is an order of magnitude
larger, so the page loads it on demand and nobody who never opens that
calculator pays for it.

Win and tie rates are both sent; losing is what is left over. That is not a
space trick, it is so the page can show wins, ties and losses separately, as
the project insists on everywhere: a hand that wins less and ties more is a
different hand, and one equity figure hides that.
"""

import csv
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUILD = REPO / "build"
WEB = REPO / "web"

SCALE = 10_000          # probabilities stored in ten-thousandths
RANGE_PERCENTS = [5, 10, 15, 20, 30, 50, 100]
MAX_OPPONENTS = 8

CATEGORIES = [
    "high_card", "pair", "two_pair", "three_of_a_kind", "straight",
    "flush", "full_house", "four_of_a_kind", "straight_flush",
]
CATEGORY_NAMES = [
    "high card", "pair", "two pair", "three of a kind", "straight",
    "flush", "full house", "four of a kind", "straight flush",
]

failures = []
worst_rounding = 0.0


def report(passed, description, detail=""):
    print(f"  {'ok  ' if passed else 'FAIL'}  {description}")
    if detail:
        print(f"        {detail}")
    if not passed:
        failures.append(description)


def read_csv(path):
    with open(path) as handle:
        return list(csv.DictReader(line for line in handle if not line.startswith("#")))


def quantise(value):
    """A probability in [0,1] as an integer in ten-thousandths, rounding error tracked."""
    global worst_rounding
    scaled = round(value * SCALE)
    worst_rounding = max(worst_rounding, abs(value - scaled / SCALE))
    return scaled


def build_hands():
    """One record per starting hand, holding everything the page needs per hand."""
    potential = {row["hand"]: row for row in read_csv(BUILD / "potential_169_exact.csv")}
    refined = {row["hand"]: row for row in read_csv(BUILD / "potential_refined_169_exact.csv")}
    exact_random = {row["hand"]: row for row in read_csv(BUILD / "equity_169_vs_random_exact.csv")}
    ranking = {row["hand"]: row for row in read_csv(BUILD / "hand_ranking_exact.csv")}
    landscape = {row["hand"]: row for row in read_csv(BUILD / "equity_landscape_169_exact.csv")}

    versus_opponents = {}
    for row in read_csv(BUILD / "equity_169_vs_1to8_mc.csv"):
        versus_opponents.setdefault(row["hand"], {})[int(row["opponents"])] = row

    versus_ranges = {}
    for row in read_csv(BUILD / "equity_169_vs_ranges_exact.csv"):
        versus_ranges.setdefault(row["hand"], {})[int(row["range_percent"])] = row

    beaten = {}
    for row in read_csv(BUILD / "beaten_at_least_one.csv"):
        beaten.setdefault(row["hand"], {})[int(row["opponents"])] = row

    boards = int(next(iter(potential.values()))["boards"])

    records = []
    for hand in sorted(ranking, key=lambda h: int(ranking[h]["rank"])):
        rank_row = ranking[hand]
        potential_row = potential[hand]
        refined_row = refined[hand]

        records.append({
            "hand": hand,
            "family": rank_row["family"],
            "combos": int(rank_row["combos"]),
            "rank": int(rank_row["rank"]),
            # The cumulative share of all 1,326 starting hands this hand and
            # everything above it make up. This is what defines a "top X%".
            "percentile": quantise(float(rank_row["cumulative_percent"]) / 100),

            # Exact, from the matrix.
            "equityVsRandom": quantise(float(exact_random[hand]["equity"])),
            "winVsRandom": quantise(float(exact_random[hand]["win_rate"])),
            "tieVsRandom": quantise(float(exact_random[hand]["tie_rate"])),

            # Estimated, one entry per opponent count, with its standard error.
            "equityVsOpponents": [
                quantise(float(versus_opponents[hand][n]["equity"]))
                for n in range(1, MAX_OPPONENTS + 1)
            ],
            "equityVsOpponentsError": [
                quantise(float(versus_opponents[hand][n]["standard_error"]))
                for n in range(1, MAX_OPPONENTS + 1)
            ],

            # Exact, one entry per range.
            "equityVsRanges": [
                quantise(float(versus_ranges[hand][percent]["equity"]))
                for percent in RANGE_PERCENTS
            ],

            # Exact potential: the nine categories, as probabilities.
            "potential": [
                quantise(int(potential_row[category]) / boards)
                for category in CATEGORIES
            ],
            # The same, requiring the hand to use one of your own cards.
            "potentialUsingOwnCard": [
                quantise(int(refined_row[f"hole_{name}"]) / boards)
                for name in CATEGORY_NAMES
            ],
            # How your cards improve on the board: four exclusive buckets.
            "improvement": [
                quantise(int(refined_row[column]) / boards)
                for column in ("raises_category", "raises_defining_rank",
                               "improves_kicker_only", "no_improvement")
            ],

            # Exact: of the 1,225 hands an opponent can hold, how many beat
            # yours, split with it, and lose to it.
            "opponentHands": {
                "beatYou": int(landscape[hand]["beats_hero"]),
                "splitWithYou": int(landscape[hand]["splits_with_hero"]),
                "loseToYou": int(landscape[hand]["hero_favourite"]),
                "lowestEquity": quantise(float(landscape[hand]["lowest_equity"])),
                "medianEquity": quantise(float(landscape[hand]["median_equity"])),
                "highestEquity": quantise(float(landscape[hand]["highest_equity"])),
                "bands": [int(landscape[hand][f"upto_{edge}_percent"])
                          for edge in (20, 35, 45, 50, 55, 65, 80, 100)],
            },

            # The chance at least one of n opponents holds a hand that beats
            # yours, next to the shortcut that assumes independence.
            "atLeastOneBeatsYou": [
                quantise(float(beaten[hand][n]["at_least_one"]))
                for n in range(1, MAX_OPPONENTS + 1)
            ],
            "atLeastOneIndependentApproximation": [
                quantise(float(beaten[hand][n]["independent_approximation"]))
                for n in range(1, MAX_OPPONENTS + 1)
            ],
            "atLeastOneIsExact": [
                beaten[hand][n]["method"] == "exact"
                for n in range(1, MAX_OPPONENTS + 1)
            ],
        })

    return records, boards


def build_matrix(hand_order):
    """The 169 x 169 head-to-head matrix, row-major, as win and tie rates."""
    cells = {}
    for row in read_csv(BUILD / "headsup_169x169_exact.csv"):
        boards = int(row["boards"])
        cells[(row["hero"], row["opponent"])] = (
            int(row["wins"]) / boards, int(row["ties"]) / boards
        )

    wins, ties = [], []
    for hero in hand_order:
        for opponent in hand_order:
            win, tie = cells[(hero, opponent)]
            wins.append(quantise(win))
            ties.append(quantise(tie))

    return {"hands": hand_order, "win": wins, "tie": ties}


def provenance():
    """Where every figure came from, carried with the data rather than beside it."""
    seeds = {}
    for name, path in (("equityVsOpponents", "equity_169_vs_1to8_mc.csv"),
                       ("atLeastOneBeatsYou", "beaten_by_k.csv")):
        with open(BUILD / path) as handle:
            seeds[name] = "".join(line for line in handle if line.startswith("#"))

    return {
        "scale": SCALE,
        "scaleNote": ("Every probability is an integer in ten-thousandths: "
                      "divide by 10000. Rounding is bounded by half a "
                      "ten-thousandth, which is finer than any figure shown and "
                      "finer than the simulation's own standard error."),
        "exact": [
            "equityVsRandom", "winVsRandom", "tieVsRandom", "equityVsRanges",
            "potential", "potentialUsingOwnCard", "improvement", "opponentHands",
            "rank", "percentile", "headsup matrix win and tie",
        ],
        "estimated": [
            "equityVsOpponents (all 8), with equityVsOpponentsError",
            "atLeastOneBeatsYou for 5 to 8 opponents; exact for 1 to 4, "
            "see atLeastOneIsExact",
        ],
        "rangePercents": RANGE_PERCENTS,
        "categoryNames": CATEGORY_NAMES,
        "improvementNames": [
            "better category than the board makes",
            "same category, bigger combination",
            "same combination, better side card",
            "no improvement on the board",
        ],
        "equityBandUpperPercents": [20, 35, 45, 50, 55, 65, 80, 100],
        "notShipped": ("The all-in tables. Every entry is two lines of arithmetic "
                       "over equityVsRanges and the stack depth, so the page "
                       "computes them live from the formulas instead."),
        "sourceHeaders": seeds,
    }


def validate(records, matrix, boards):
    """Checks the repack against the tables it came from.

    The point of repacking is that nothing changes except the shape, so every
    check here asks whether something that was true in build/ is still true.
    """
    print("\nChecking the repack")

    report(len(records) == 169 and len(matrix["win"]) == 169 * 169,
           "169 hands and all 28,561 matrix cells are present",
           f"{len(records)} hands, {len(matrix['win'])} cells")

    report(sum(record["combos"] for record in records) == 1326,
           "the hands account for all 1,326 concrete starting hands")

    # Half a ten-thousandth is the arithmetic limit of rounding to the nearest
    # ten-thousandth, and some value always lands exactly on it, so the
    # comparison needs a hair of slack for floating-point representation. What
    # would fail here is a packing bug that scaled something wrongly, which
    # would overshoot the limit by orders of magnitude, not by 1e-16.
    limit = 0.5 / SCALE
    report(worst_rounding <= limit + 1e-12,
           "rounding never exceeded half a ten-thousandth, the arithmetic limit",
           f"worst was {worst_rounding * 100:.8f} percentage points, "
           f"against a limit of {limit * 100:.8f}")

    # The nine potential categories are exclusive and exhaustive, so they must
    # still sum to one after rounding, give or take the rounding itself.
    off = [record["hand"] for record in records
           if abs(sum(record["potential"]) - SCALE) > 5]
    report(not off, "the nine potential categories still sum to 1",
           "" if not off else f"off for {', '.join(off[:5])}")

    # So must the four improvement buckets.
    off = [record["hand"] for record in records
           if abs(sum(record["improvement"]) - SCALE) > 5]
    report(not off, "the four improvement buckets still sum to 1",
           "" if not off else f"off for {', '.join(off[:5])}")

    # And the three classifications of opponent hands must still be 1,225.
    off = [record["hand"] for record in records
           if sum((record["opponentHands"]["beatYou"],
                   record["opponentHands"]["splitWithYou"],
                   record["opponentHands"]["loseToYou"])) != 1225]
    report(not off, "every hand still classifies all 1,225 opponent hands")

    # The equity bands must also account for all 1,225.
    off = [record["hand"] for record in records
           if sum(record["opponentHands"]["bands"]) != 1225]
    report(not off, "the equity bands still account for all 1,225 opponent hands")

    # The matrix must still be its own mirror, and still average to one half.
    # After rounding these hold to within the rounding, not exactly, so the
    # tolerance is one ten-thousandth either way.
    hands = matrix["hands"]
    index = {hand: i for i, hand in enumerate(hands)}
    mismatches = 0
    for a, hero in enumerate(hands):
        for b in range(a + 1, len(hands)):
            forward = a * 169 + b
            backward = b * 169 + a
            win_against_loss = matrix["win"][forward] + matrix["tie"][forward] \
                + matrix["win"][backward]
            if abs(win_against_loss - SCALE) > 2 or \
                    matrix["tie"][forward] != matrix["tie"][backward]:
                mismatches += 1
    report(mismatches == 0,
           "the matrix is still its own mirror: wins against losses, ties shared",
           "" if mismatches == 0 else f"{mismatches} pairs disagree")

    total_equity = sum(win + tie / 2 for win, tie in zip(matrix["win"], matrix["tie"]))
    average = total_equity / len(matrix["win"]) / SCALE
    report(abs(average - 0.5) < 1e-4,
           "the matrix still averages to one half",
           f"it averages to {average:.8f}")

    # A hand's row of the matrix, averaged with the right weights, must come
    # back to its exact equity against a random hand. This is the check that
    # the matrix and the per-hand records describe the same thing, and that the
    # hand order in one matches the other.
    deals = {}
    for row in read_csv(BUILD / "headsup_169x169_exact.csv"):
        deals[(row["hero"], row["opponent"])] = int(row["boards"])

    worst_hand, worst_gap = None, 0.0
    for record in records:
        hero = record["hand"]
        numerator = denominator = 0
        for opponent in hands:
            cell = index[opponent] + index[hero] * 169
            weight = deals[(hero, opponent)]
            equity = (matrix["win"][cell] + matrix["tie"][cell] / 2) / SCALE
            numerator += weight * equity
            denominator += weight
        gap = abs(numerator / denominator - record["equityVsRandom"] / SCALE)
        if gap > worst_gap:
            worst_hand, worst_gap = hero, gap

    report(worst_gap < 1e-4,
           "every hand's matrix row averages back to its equity against a random hand",
           f"worst gap {worst_gap * 100:.6f} pp, at {worst_hand}")

    # And the per-hand estimated figures must still sit within their own error
    # of the exact ones, which is the only check here that can fail for a
    # reason other than a packing bug.
    off = []
    for record in records:
        estimate = record["equityVsOpponents"][0] / SCALE
        error = record["equityVsOpponentsError"][0] / SCALE
        exact = record["equityVsRandom"] / SCALE
        if error > 0 and abs(estimate - exact) > 5 * error:
            off.append(record["hand"])
    report(not off,
           "the one-opponent estimates still sit within five standard errors of exact",
           "" if not off else f"{', '.join(off[:5])}")


def write_dictionary(records, boards):
    """Writes the field-by-field dictionary that ships with the data."""
    path = WEB / "DATA_DICTIONARY.md"
    path.write_text(f"""# What is in these files

Two files, both produced by `tools/build_web_data.py` from the tables in
`build/`. The repack adds no information: every number here appears in
`build/` as well, in fuller form, and `build/` is the archive to check against.

**Every probability is an integer in ten-thousandths.** Divide by
{SCALE}. So `6712` means 0.6712, or 67.12%. Rounding is bounded by half a
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
| `equityVsRanges` | **exact** | Seven entries, against an opponent playing the top {', '.join(f'{p}%' for p in RANGE_PERCENTS)} of hands |
| `potential` | **exact** | Nine entries, the chance of finishing in each hand category over all {boards:,} boards. Exclusive and exhaustive: they sum to 1. **This is not the chance of winning** |
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
""")
    return path


def main():
    needed = ["potential_169_exact.csv", "potential_refined_169_exact.csv",
              "equity_169_vs_random_exact.csv", "hand_ranking_exact.csv",
              "equity_landscape_169_exact.csv", "equity_169_vs_1to8_mc.csv",
              "equity_169_vs_ranges_exact.csv", "beaten_at_least_one.csv",
              "headsup_169x169_exact.csv"]
    missing = [name for name in needed if not (BUILD / name).exists()]
    if missing:
        print(f"Nothing to pack: {', '.join(missing)} not built. Run `make data` first.")
        return 1

    print("Packing the engine's tables for the website")

    records, boards = build_hands()
    matrix = build_matrix([record["hand"] for record in records])
    validate(records, matrix, boards)

    if failures:
        print(f"\n{len(failures)} check(s) failed. Nothing written:")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    WEB.mkdir(exist_ok=True)

    hands_path = WEB / "hands.json"
    hands_path.write_text(json.dumps(
        {"meta": provenance(), "hands": records}, separators=(",", ":")))

    matrix_path = WEB / "headsup-matrix.json"
    matrix_path.write_text(json.dumps(matrix, separators=(",", ":")))

    dictionary_path = write_dictionary(records, boards)

    print()
    for path in (hands_path, matrix_path, dictionary_path):
        size = path.stat().st_size
        print(f"Wrote {path.relative_to(REPO)}  ({size / 1024:.0f} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
