"""Verifies the engine's output against the exploration phase and against itself.

The engine already checks itself from the inside: the evaluator's test suite
enumerates whole hand spaces, and each generator validates its own table before
writing it. This script checks the two things those cannot:

1.  REPRODUCTION. The exact potential counts must match the exploration phase
    cell for cell. Those counts were produced by a different program, written
    independently, so agreement on all 1,859 cells means two implementations
    reached the same answer. Any disagreement is a real finding and is printed
    in full rather than summarised.

2.  HONEST ERROR BARS. The Monte Carlo table reports a standard error for every
    cell, and a reported error is a claim that has to be tested. Two runs with
    different seeds are independent samples of the same quantity, so their
    difference divided by the combined error should behave like a standard
    normal: mean 0, standard deviation 1, about 5% of cells beyond 1.96 and 1%
    beyond 2.58. If the errors were understated, that spread would come out
    wider than 1; if overstated, narrower.

    The same comparison is then made against the exploration phase's own
    100,000-trial run, which reported no per-cell error. Its stated bound of
    0.16 percentage points is used in its place, which makes this the more
    conservative of the two comparisons: the bound is larger than the true
    error for lopsided hands, so the spread comes out slightly under 1.

Exits non-zero if anything fails, so `make verify` fails with it.
"""

import csv
import math
import statistics
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUILD = REPO / "build"
EXPLORATION = REPO / "exploration" / "data"

# The exploration phase wrote its column names in Spanish. Reproduction is
# about the numbers, so the two namings are mapped here rather than one of them
# being bent to match the other.
POTENTIAL_COLUMNS = [
    ("carta_alta", "high_card"),
    ("pareja", "pair"),
    ("doble_pareja", "two_pair"),
    ("trio", "three_of_a_kind"),
    ("escalera", "straight"),
    ("color", "flush"),
    ("full_house", "full_house"),
    ("poker", "four_of_a_kind"),
    ("escalera_color", "straight_flush"),
    ("mejora_mesa", "improves_board"),
]

# The per-cell standard error the exploration phase claimed for its run. It
# reported no column of its own, so this bound stands in for one.
EXPLORATION_STANDARD_ERROR = 0.0016

failures = []


def report(passed, description, detail=""):
    print(f"  {'ok  ' if passed else 'FAIL'}  {description}")
    if detail:
        print(f"        {detail}")
    if not passed:
        failures.append(description)


def read_csv(path):
    """Reads a CSV, skipping the comment lines the generators write on top."""
    with open(path) as handle:
        return list(csv.DictReader(line for line in handle if not line.startswith("#")))


def verify_exact_potential():
    print("\nExact potential: engine vs the exploration phase")

    engine = read_csv(BUILD / "potential_169_exact.csv")
    reference = {row["mano"]: row for row in read_csv(EXPLORATION / "potencial_169_conteos_exactos.csv")}

    report(len(engine) == 169, "the engine produced 169 hand types",
           "" if len(engine) == 169 else f"produced {len(engine)}")
    report(len(reference) == 169, "the reference holds 169 hand types",
           "" if len(reference) == 169 else f"holds {len(reference)}")

    mismatches = []
    cells = 0
    for row in engine:
        hand = row["hand"]
        if hand not in reference:
            mismatches.append(f"{hand} is missing from the reference")
            continue

        other = reference[hand]
        if int(row["combos"]) != int(other["combos"]):
            mismatches.append(f"{hand} combos: engine {row['combos']}, reference {other['combos']}")
        cells += 1

        for spanish, english in POTENTIAL_COLUMNS:
            cells += 1
            if int(row[english]) != int(other[spanish]):
                mismatches.append(
                    f"{hand} {english}: engine {row[english]}, reference {other[spanish]}"
                )

    report(not mismatches, f"all {cells} counts match the exploration phase exactly")
    for mismatch in mismatches[:20]:
        print(f"        {mismatch}")
    if len(mismatches) > 20:
        print(f"        ... and {len(mismatches) - 20} more")


def load_equity(path):
    return {
        (row["hand"], int(row["opponents"])): (float(row["equity"]), float(row["standard_error"]))
        for row in read_csv(path)
    }


def describe_spread(z_scores, label, expected_spread_note):
    """Reports how a set of z-scores is distributed, against what it should be."""
    mean = statistics.mean(z_scores)
    spread = statistics.pstdev(z_scores)
    beyond_196 = 100 * sum(1 for z in z_scores if abs(z) > 1.96) / len(z_scores)
    beyond_258 = 100 * sum(1 for z in z_scores if abs(z) > 2.58) / len(z_scores)
    largest = max(abs(z) for z in z_scores)

    print(f"\n  {label} ({len(z_scores)} cells)")
    print(f"    mean of z           {mean:+.4f}   expected  0")
    print(f"    standard deviation  {spread:.4f}   expected  1 {expected_spread_note}")
    print(f"    beyond 1.96         {beyond_196:.1f} %     expected  5.0 %")
    print(f"    beyond 2.58         {beyond_258:.1f} %     expected  1.0 %")
    print(f"    largest |z|         {largest:.2f}")

    return mean, spread, largest


def verify_monte_carlo_errors():
    print("\nMonte Carlo: are the reported standard errors honest?")

    first = load_equity(BUILD / "equity_169_vs_1to8_mc.csv")
    second = load_equity(BUILD / "equity_169_vs_1to8_mc_seed2.csv")

    report(first.keys() == second.keys(), "both runs cover the same 1,352 cells")

    z_scores = []
    for key in first:
        equity_a, error_a = first[key]
        equity_b, error_b = second[key]
        z_scores.append((equity_a - equity_b) / math.hypot(error_a, error_b))

    mean, spread, largest = describe_spread(
        z_scores, "two seeds against each other", ""
    )

    # Thresholds are deliberately loose: with 1,352 cells these statistics have
    # their own sampling error, and the point is to catch errors that are wrong
    # by a lot, not to re-estimate them to three decimals.
    report(abs(mean) < 0.15, "the two runs agree on average, with no systematic offset",
           f"mean z is {mean:+.4f}")
    report(0.85 < spread < 1.18, "the spread matches the reported errors",
           f"standard deviation of z is {spread:.4f}")
    report(largest < 5.0, "no cell disagrees beyond what sampling explains",
           f"largest |z| is {largest:.2f}")

    # Now against the exploration run.
    reference = {}
    for row in read_csv(EXPLORATION / "equity_169_vs_1a8_rivales_montecarlo.csv"):
        for opponents in range(1, 9):
            reference[(row["mano"], opponents)] = float(row[f"eq_{opponents}r"])

    z_scores = []
    worst_key, worst_z = None, 0.0
    for key in first:
        equity, error = first[key]
        z = (equity - reference[key]) / math.hypot(error, EXPLORATION_STANDARD_ERROR)
        z_scores.append(z)
        if abs(z) > abs(worst_z):
            worst_key, worst_z = key, z

    mean, spread, largest = describe_spread(
        z_scores, "engine against the exploration phase",
        "or a little under, see the note above",
    )
    hand, opponents = worst_key
    print(f"    widest gap          {hand} vs {opponents}: engine "
          f"{first[worst_key][0]:.4f}, exploration {reference[worst_key]:.4f}")

    report(abs(mean) < 0.15, "the engine reproduces the exploration equities on average",
           f"mean z is {mean:+.4f}")
    report(spread < 1.18, "no cell-level disagreement beyond sampling error",
           f"standard deviation of z is {spread:.4f}")
    report(largest < 5.0, "the widest single gap is explained by sampling",
           f"largest |z| is {largest:.2f}")


# Equities of a few hands against one random opponent, as published in the
# standard reference tables. Rounded to a tenth of a point, which is the
# precision they are usually quoted to, so the comparison is made at that
# precision and no tighter.
PUBLISHED_VERSUS_RANDOM = {
    "AA": 85.2, "KK": 82.4, "QQ": 79.9, "JJ": 77.5, "TT": 75.0,
    "AKs": 67.0, "AKo": 65.3, "22": 50.3, "98s": 50.8, "72o": 34.6,
}


def verify_exact_matrix():
    """Checks the exact matrix against the simulation and against outside figures.

    The matrix validates its own invariants in integer arithmetic before it is
    written, so what is left to check is whether it agrees with anything
    outside itself. Two comparisons do that.

    First, the Monte Carlo equity against one opponent now has an exact answer
    to be measured against, which is a stronger test than comparing two
    simulations: an estimate and the truth, divided by the estimate's own
    standard error, should behave like a standard normal. This tests the
    simulation and the error bars together, against a number that was not
    sampled.

    Second, a handful of hands have equities that have been published for
    decades. Reproducing them to the tenth of a point they are quoted to ties
    the whole chain — evaluator, enumeration, suit isomorphism, weighting — to
    figures computed by other people with other code.
    """
    print("\nExact head-to-head matrix: against the simulation, and against published figures")

    exact = {row["hand"]: row for row in read_csv(BUILD / "equity_169_vs_random_exact.csv")}
    estimated = {
        row["hand"]: row
        for row in read_csv(BUILD / "equity_169_vs_1to8_mc.csv")
        if row["opponents"] == "1"
    }

    report(len(exact) == 169, "the matrix yields all 169 hands against a random hand")

    z_scores = []
    for hand, row in exact.items():
        truth = float(row["equity"])
        estimate = float(estimated[hand]["equity"])
        error = float(estimated[hand]["standard_error"])
        z_scores.append((estimate - truth) / error)

    mean, spread, largest = describe_spread(
        z_scores, "the simulation against exact truth", ""
    )
    report(abs(mean) < 0.2, "the simulation is unbiased against the exact values",
           f"mean z is {mean:+.4f}")
    report(0.8 < spread < 1.25, "the simulation's error bars match its actual error",
           f"standard deviation of z is {spread:.4f}")
    report(largest < 5.0, "no hand is estimated further off than sampling explains",
           f"largest |z| is {largest:.2f}")

    print()
    worst_gap = 0.0
    for hand, published in PUBLISHED_VERSUS_RANDOM.items():
        computed = float(exact[hand]["equity"]) * 100
        gap = abs(computed - published)
        worst_gap = max(worst_gap, gap)
        print(f"    {hand:4s} computed {computed:6.3f} %   published {published:5.1f} %   "
              f"gap {gap:.3f} pp")

    report(worst_gap < 0.06,
           f"all {len(PUBLISHED_VERSUS_RANDOM)} published equities are reproduced "
           f"to the precision they are quoted to",
           f"widest gap is {worst_gap:.3f} pp")


def main():
    print("Verifying the engine's output")
    print("=============================")

    missing = [
        path.name
        for path in (
            BUILD / "potential_169_exact.csv",
            BUILD / "equity_169_vs_1to8_mc.csv",
            BUILD / "equity_169_vs_1to8_mc_seed2.csv",
            BUILD / "equity_169_vs_random_exact.csv",
        )
        if not path.exists()
    ]
    if missing:
        print(f"\nNothing to verify: {', '.join(missing)} not built. Run `make data` first.")
        return 1

    verify_exact_potential()
    verify_monte_carlo_errors()
    verify_exact_matrix()

    print(f"\n{'All checks passed.' if not failures else str(len(failures)) + ' check(s) failed:'}")
    for failure in failures:
        print(f"  - {failure}")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
