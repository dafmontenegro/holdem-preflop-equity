"""Derives the hand ranking and equity against "top X%" ranges from the exact matrix.

NOTHING HERE IS ESTIMATED, AND NOTHING IS RE-ENUMERATED
-------------------------------------------------------
Both tables are sums of the counts already in the exact head-to-head matrix,
so they are exact too. The arithmetic is done in Python's `Fraction`, which is
exact rational arithmetic: no rounding enters until a figure is written out at
nine decimal places. That is what lets the checks below be equalities rather
than comparisons against a tolerance.

This is a derivation rather than a computation, which is why it is a script
and not another C program. The enumeration it rests on is in src/headsup.c.

WHAT A "TOP X%" RANGE IS
------------------------
Opponents do not call with every hand. A range is the set of hands an opponent
is willing to play, and "top X%" is the conventional shorthand: rank all 1,326
concrete starting hands by strength and take the best X% of them.

Strength here means **exact equity against one opponent holding a random
hand**, which is the only ordering in this project that is computed rather than
asserted. It is a defensible ranking and not the only one: real ranges are
shaped by position, stack depth and opponent, and good players deviate from any
pure equity order, most famously by playing suited connectors ahead of hands
that outrank them against a random hand. The ranking is published as its own
file so this choice is visible and can be argued with.

Ranges are built out of whole hand types, never split. A type is 6, 4 or 12 of
the 1,326 hands, so the boundary rarely lands exactly on X%, and the table
records the percentage each range actually covers alongside the one it was
asked for. Splitting a type would mean claiming an opponent plays AKo from
three suit combinations and folds the fourth, which is not a thing anyone does.

HOW EQUITY AGAINST A RANGE IS COMPUTED
--------------------------------------
For hero hand H and range R, the equity is

    (sum over T in R of 2*wins[H][T] + ties[H][T])  /  (sum over T in R of 2*boards[H][T])

Weighting by `boards` rather than by the number of hand types is what makes
this the equity against an opponent whose hand is drawn uniformly from R and
shares no card with the hero. It also handles card removal for free: a player
holding AA blocks all but one of the six ways an opponent can hold AA, and
because `boards` counts concrete deals, that single remaining combination is
all the weight AA gets in the opponent's range.
"""

import csv
import sys
from fractions import Fraction
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUILD = REPO / "build"

CONCRETE_HANDS = 1326          # C(52,2)
BOARDS_PER_DEAL = 1712304      # C(48,5)

# The range sizes the project reports. 100% is kept because it is the check:
# equity against the top 100% must equal equity against a random hand.
RANGE_PERCENTS = [5, 10, 15, 20, 30, 50, 100]

failures = []


def report(passed, description, detail=""):
    print(f"  {'ok  ' if passed else 'FAIL'}  {description}")
    if detail:
        print(f"        {detail}")
    if not passed:
        failures.append(description)


def read_csv(path):
    with open(path) as handle:
        return list(csv.DictReader(line for line in handle if not line.startswith("#")))


def load_matrix():
    """Loads the exact matrix as integer counts, keyed by (hero, opponent)."""
    cells = {}
    for row in read_csv(BUILD / "headsup_169x169_exact.csv"):
        cells[(row["hero"], row["opponent"])] = (
            int(row["wins"]), int(row["ties"]), int(row["boards"])
        )
    return cells


def load_versus_random():
    return {row["hand"]: row for row in read_csv(BUILD / "equity_169_vs_random_exact.csv")}


def build_ranking(versus_random):
    """Orders the 169 types by exact equity against a random hand, best first.

    Ties are broken by hand label so the order is reproducible rather than
    dependent on dictionary iteration; no two hands actually tie.
    """
    hands = sorted(
        versus_random.values(),
        key=lambda row: (-Fraction(int(row["wins"]) * 2 + int(row["ties"]),
                                   int(row["boards"]) * 2), row["hand"]),
    )

    ranking = []
    cumulative = 0
    for position, row in enumerate(hands, start=1):
        cumulative += int(row["combos"])
        ranking.append({
            "rank": position,
            "hand": row["hand"],
            "family": row["family"],
            "combos": int(row["combos"]),
            "equity": Fraction(int(row["wins"]) * 2 + int(row["ties"]),
                               int(row["boards"]) * 2),
            "cumulative_combos": cumulative,
            "cumulative_percent": Fraction(cumulative * 100, CONCRETE_HANDS),
        })
    return ranking


def range_of(ranking, percent):
    """The hand types making up the top `percent` of all starting hands.

    Types are added in ranking order while the range still covers less than
    the target, so the result is the shortest prefix of the ranking that
    reaches `percent`. Returns the types and the share they actually cover.
    """
    if percent >= 100:
        covered = Fraction(100)
        return [entry["hand"] for entry in ranking], covered

    hands = []
    for entry in ranking:
        hands.append(entry["hand"])
        if entry["cumulative_percent"] >= percent:
            return hands, entry["cumulative_percent"]

    return hands, Fraction(100)


def equity_against(cells, hero, range_hands):
    """Exact equity of `hero` against an opponent drawn uniformly from a range."""
    numerator = 0
    denominator = 0
    for opponent in range_hands:
        wins, ties, boards = cells[(hero, opponent)]
        numerator += 2 * wins + ties
        denominator += 2 * boards
    return Fraction(numerator, denominator)


def write_ranking(ranking):
    path = BUILD / "hand_ranking_exact.csv"
    with open(path, "w") as out:
        out.write("# The 169 starting hands ordered by exact equity against one opponent\n")
        out.write("# holding a uniformly random hand. Derived from the exact matrix.\n")
        out.write("hand,family,combos,rank,equity_vs_random,"
                  "cumulative_combos,cumulative_percent\n")
        for entry in ranking:
            out.write(f"{entry['hand']},{entry['family']},{entry['combos']},"
                      f"{entry['rank']},{float(entry['equity']):.9f},"
                      f"{entry['cumulative_combos']},"
                      f"{float(entry['cumulative_percent']):.4f}\n")
    return path


def write_ranges(ranking, cells, ranges):
    path = BUILD / "equity_169_vs_ranges_exact.csv"
    with open(path, "w") as out:
        out.write("# Exact equity of each starting hand against one opponent playing\n")
        out.write("# the top X% of hands. Ranges are whole hand types, so each one\n")
        out.write("# records the share it actually covers. Derived from the exact matrix.\n")
        out.write("hand,family,combos,range_percent,range_covers_percent,"
                  "range_types,equity\n")
        for entry in ranking:
            for percent in RANGE_PERCENTS:
                range_hands, covered = ranges[percent]
                equity = equity_against(cells, entry["hand"], range_hands)
                out.write(f"{entry['hand']},{entry['family']},{entry['combos']},"
                          f"{percent},{float(covered):.4f},{len(range_hands)},"
                          f"{float(equity):.9f}\n")
    return path


def validate(ranking, cells, ranges, versus_random):
    print("\nChecking the derivation")

    # 1. The ranking must account for all 1,326 concrete hands.
    total = sum(entry["combos"] for entry in ranking)
    report(total == CONCRETE_HANDS and len(ranking) == 169,
           f"the ranking covers all {CONCRETE_HANDS} concrete hands across 169 types",
           "" if total == CONCRETE_HANDS else f"covers {total} across {len(ranking)} types")

    # 2. Equity against the top 100% must *equal* equity against a random hand.
    #    Both are sums over the same cells, so in exact arithmetic this is an
    #    identity, and any difference means the weighting is wrong.
    all_hands = ranges[100][0]
    mismatches = 0
    for entry in ranking:
        against_everything = equity_against(cells, entry["hand"], all_hands)
        row = versus_random[entry["hand"]]
        exact = Fraction(int(row["wins"]) * 2 + int(row["ties"]), int(row["boards"]) * 2)
        if against_everything != exact:
            mismatches += 1
    report(mismatches == 0,
           "equity against the top 100% equals equity against a random hand, exactly",
           "" if mismatches == 0 else f"{mismatches} hands disagree")

    # 3. The combo-weighted average over all hands, against the full range,
    #    must be exactly one half: two random hands, no advantage either way.
    average = sum(entry["combos"] * equity_against(cells, entry["hand"], all_hands)
                  for entry in ranking) / CONCRETE_HANDS
    report(average == Fraction(1, 2),
           "the ranking's weighted average equity is exactly 1/2",
           f"it is {average}")

    # 4. Each range must be a prefix of the ranking, and they must nest.
    nested = all(set(ranges[smaller][0]) <= set(ranges[larger][0])
                 for smaller, larger in zip(RANGE_PERCENTS, RANGE_PERCENTS[1:]))
    report(nested, "every range is contained in the next one up")

    # 5. A range asked for X% must cover at least X%, and must not overshoot by
    #    more than one hand type's worth (12 of 1,326, under one point).
    overshoot_ok = True
    for percent in RANGE_PERCENTS:
        covered = ranges[percent][1]
        if covered < percent or covered - percent > Fraction(12 * 100, CONCRETE_HANDS):
            overshoot_ok = False
    report(overshoot_ok,
           "every range reaches its target without overshooting by more than one type")


def main():
    needed = [BUILD / "headsup_169x169_exact.csv", BUILD / "equity_169_vs_random_exact.csv"]
    missing = [path.name for path in needed if not path.exists()]
    if missing:
        print(f"Nothing to derive: {', '.join(missing)} not built. Run `make data` first.")
        return 1

    print("Deriving the hand ranking and the top X% ranges from the exact matrix")

    cells = load_matrix()
    versus_random = load_versus_random()
    ranking = build_ranking(versus_random)
    ranges = {percent: range_of(ranking, percent) for percent in RANGE_PERCENTS}

    print(f"\n  {'range':>8}  {'covers':>8}  {'types':>6}  tightest hand included")
    for percent in RANGE_PERCENTS:
        range_hands, covered = ranges[percent]
        print(f"  {percent:>7}%  {float(covered):>7.2f}%  {len(range_hands):>6}  "
              f"{range_hands[-1]}")

    validate(ranking, cells, ranges, versus_random)

    if failures:
        print(f"\n{len(failures)} check(s) failed. No output written:")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print()
    for path in (write_ranking(ranking), write_ranges(ranking, cells, ranges)):
        print(f"Wrote {path.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
