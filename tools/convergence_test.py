"""Checks that the simulation converges on the exact answer at the rate it should.

The Monte Carlo equity table reports a standard error per cell, and
verify_reproduction.py already checks that those errors describe the spread
that is really there. This checks something related but different: that
running more trials actually buys accuracy, and buys it at the rate the
mathematics predicts.

Sampling error falls as one over the square root of the number of trials. So
quadrupling the trials should halve the typical error, and the ratio between
successive runs should come out near 2. A simulation that stopped improving —
because of a biased generator, a shuffle that does not reach every deal, or an
accumulator losing precision — would show a ratio drifting towards 1 while
every individual run still looked plausible.

The measurement is against the exact equity of each hand against one
opponent, taken from the head-to-head matrix, so this compares the estimate
against the truth and not against another estimate. Only the one-opponent
column can be checked this way, because it is the only one with an exact
counterpart; it is also the only one that needs checking, since all eight
columns come out of the same generator and the same shuffle.

Each run uses a different seed, so the runs are independent rather than
nested, and a lucky early run cannot flatter the ones after it.
"""

import csv
import math
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUILD = REPO / "build"
BINARY = REPO / "bin" / "equity_mc"

TRIAL_COUNTS = [25_000, 100_000, 400_000]

failures = []


def read_csv(path):
    with open(path) as handle:
        return list(csv.DictReader(line for line in handle if not line.startswith("#")))


def exact_equities():
    return {
        row["hand"]: (2 * int(row["wins"]) + int(row["ties"])) / (2 * int(row["boards"]))
        for row in read_csv(BUILD / "equity_169_vs_random_exact.csv")
    }


def run_simulation(trials, seed, destination):
    subprocess.run(
        [str(BINARY), str(trials), str(seed), str(destination)],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    return {
        row["hand"]: (float(row["equity"]), float(row["standard_error"]))
        for row in read_csv(destination)
        if row["opponents"] == "1"
    }


def main():
    for path in (BINARY, BUILD / "equity_169_vs_random_exact.csv"):
        if not path.exists():
            print(f"Nothing to test: {path.name} not built. Run `make data` first.")
            return 1

    truth = exact_equities()
    scratch = BUILD / "convergence_scratch.csv"

    print("Does the simulation converge on the exact answer?")
    print("=================================================")
    print("\nRoot-mean-square error against the exact one-opponent equities, over")
    print("all 169 hands. Quadrupling the trials should halve it.\n")
    print(f"  {'trials':>9}  {'rms error':>11}  {'reported error':>14}  {'ratio':>7}")

    measurements = []
    for index, trials in enumerate(TRIAL_COUNTS):
        estimates = run_simulation(trials, 1000 + index, scratch)

        squared = sum((estimate - truth[hand]) ** 2
                      for hand, (estimate, _) in estimates.items())
        rms = math.sqrt(squared / len(estimates))

        # What the runs claim their own typical error is, for comparison.
        reported = math.sqrt(sum(error ** 2 for _, error in estimates.values())
                             / len(estimates))

        ratio = (measurements[-1][1] / rms) if measurements else None
        measurements.append((trials, rms, reported))

        print(f"  {trials:>9,}  {rms * 100:>10.4f} pp  {reported * 100:>13.4f} pp  "
              f"{'—' if ratio is None else f'{ratio:>7.2f}'}")

    scratch.unlink(missing_ok=True)

    print()

    # 1. The error must actually fall, and at close to the predicted rate. The
    #    tolerance is loose because an rms over 169 hands has sampling error of
    #    its own; what would fail is a ratio near 1.
    for (trials_before, rms_before, _), (trials_after, rms_after, _) in zip(
            measurements, measurements[1:]):
        expected = math.sqrt(trials_after / trials_before)
        ratio = rms_before / rms_after
        passed = 0.7 * expected < ratio < 1.4 * expected

        print(f"  {'ok  ' if passed else 'FAIL'}  going from {trials_before:,} to "
              f"{trials_after:,} trials cut the error by {ratio:.2f}x, against "
              f"{expected:.2f}x predicted")
        if not passed:
            failures.append(f"convergence from {trials_before} to {trials_after}")

    # 2. The error the runs report must match the error they actually have. If
    #    the reported figure were optimistic, this is where it shows.
    for trials, rms, reported in measurements:
        passed = 0.75 < rms / reported < 1.35
        print(f"  {'ok  ' if passed else 'FAIL'}  at {trials:,} trials the actual "
              f"error is {rms / reported:.2f}x the reported one")
        if not passed:
            failures.append(f"reported error at {trials} trials")

    if failures:
        print(f"\n{len(failures)} check(s) failed:")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print("\nThe simulation converges on the exact answer at the predicted rate, "
          "and its\nreported errors are honest at every trial count.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
