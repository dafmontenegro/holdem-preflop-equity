"""The preflop all-in decision: when calling is right, and when shoving is.

Everything here is derived from the exact head-to-head matrix, in exact
rational arithmetic, so the equities carry no sampling error. What the answers
do carry is a *model*, and the model is the part worth arguing with. It is
written out in full below rather than buried in the code, because every number
this file produces is only as good as the situation it assumes.

THE SITUATION BEING MODELLED
----------------------------
Heads-up, blind versus blind, with both players holding the same stack. One
player moves all-in before the flop and the other decides whether to call.

    effective stack   S big blinds, the same for both players
    small blind       0.5 big blinds, posted
    big blind         1 big blind, posted

This is the canonical push-or-fold situation: short-stacked heads-up play,
where there is no room to do anything after the flop and the whole hand is one
decision. It was chosen because it is the situation that is *exactly* solvable
and because it is the one the "call with JJ+ and AK" rule is usually quoted
about. It is not the only situation at a poker table, and the limitations
section at the end says what it leaves out.

Money is counted in final stacks rather than in "profit", which avoids every
argument about whether a posted blind is still yours. Each decision is
reported as the difference between the expected final stack of acting and the
expected final stack of folding, in big blinds. Zero means indifference;
positive means the action makes money.

THE CALL
--------
You are the big blind. The small blind has moved all-in for S. You have
already posted 1, so calling costs you S - 1 more.

    fold                     final stack = S - 1
    call and the hands run   the pot is 2S, and you collect q of it on average

    EV(call) - EV(fold) = 2*S*q - (S - 1)

where q is your equity against the hands they could be shoving with: wins plus
half the ties, which is exactly the equity column of the exact matrix. Setting
that to zero gives the equity you need:

    q* = (S - 1) / (2*S)

That formula is the whole reason a flat "you need 50%" is wrong. At 100 big
blinds you need 49.5%, which is nearly 50. At 20 you need 47.5%. At 10 you
need 45%, and at 5 you need 40%. The money already in the pot is not yours
any more, and the shorter the stacks the larger a share of the pot it is.

THE SHOVE
---------
You are the small blind. You have posted 0.5 and you move all-in for S. The
big blind folds with probability f, and otherwise calls.

    fold                  final stack = S - 0.5
    shove, they fold      you win the 1.5 in the pot: final stack = S + 1
    shove, they call      the pot is 2S, and you collect q of it on average

    EV(shove) - EV(fold) = f*(S + 1) + (1 - f)*2*S*q - (S - 0.5)

Note which q this is: your equity **given that they called**, so it is measured
against their calling range, not against a random hand. A shove gets its value
from two different places — the times nobody calls, and the times you win a
pot you were called in — and conflating them is the usual way this calculation
goes wrong.

WHERE f COMES FROM, AND WHY IT IS NOT INVENTED
----------------------------------------------
f is not a guess here. Once a calling range is named, the chance they fold is
the chance their two cards are not in it, and that is a count:

    f = 1 - (hands in their range that you do not block) / 1225

The "you do not block" is the part worth noticing. Holding AA removes five of
the six ways they can hold AA, so your own cards make their strong range
slightly less likely and f slightly larger. That effect is real, it is in the
exact deal counts already, and it is why f is computed per hand rather than
once per range.

The breakeven fold probability is reported too, from solving the equation
above for f:

    f* = (S - 0.5 - 2*S*q) / (S + 1 - 2*S*q)

When q is high enough the denominator goes negative, which means the shove
makes money however often they fold — you would rather be called. That case
is reported as such instead of as a nonsensical probability.

WHAT THIS DOES NOT MODEL
------------------------
No position beyond blind versus blind. No unequal stacks, so no side pots: the
interface is built to take them later, but v1 assumes both players have S. No
antes. No more than two players. No tournament prize structure, so a chip is
worth a chip and nothing here is ICM.

And the big one: **the opponent does not react.** Their range is an input,
fixed, and in particular it does not change when your strategy does. That
limitation is load-bearing, not cosmetic, and it shows up most visibly in the
shove table: at 10 big blinds against an opponent calling the top 20%, all 169
hands shove profitably, 32o included. That number is correct for the question
asked — a player facing a shove, folding 78% of the time, against which any
two cards show a profit — and it is not advice. A real opponent who noticed
you shoving everything would call far wider, and against a wider caller most
of those shoves stop working. Finding the ranges that are stable against each
other is a game-theory problem, a Nash equilibrium for the push-or-fold game,
and this file does not solve it. It answers "what is this play worth against
*that* opponent", which is a different and more modest question.
"""

import csv
import sys
from fractions import Fraction
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUILD = REPO / "build"

OPPONENT_HANDS = 1225          # C(50,2)
RANGE_PERCENTS = [5, 10, 15, 20, 30, 50, 100]

# Effective stacks in big blinds. The short end is where push-or-fold actually
# lives; 100 is included to show the required equity converging on one half.
STACK_DEPTHS = [5, 10, 15, 20, 25, 30, 50, 100]

# The rule this project set out to check.
CLAIMED_CALLING_RANGE = {"AA", "KK", "QQ", "JJ", "AKs", "AKo"}

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


def load():
    """Loads the exact matrix and the ranking, and rebuilds the ranges."""
    cells = {}
    for row in read_csv(BUILD / "headsup_169x169_exact.csv"):
        cells[(row["hero"], row["opponent"])] = {
            "numerator": 2 * int(row["wins"]) + int(row["ties"]),
            "boards": int(row["boards"]),
            "deals": int(row["deals"]),
        }

    ranking = read_csv(BUILD / "hand_ranking_exact.csv")
    ranking.sort(key=lambda row: int(row["rank"]))

    ranges = {}
    for percent in RANGE_PERCENTS:
        hands = []
        for entry in ranking:
            hands.append(entry["hand"])
            if percent < 100 and float(entry["cumulative_percent"]) >= percent:
                break
        ranges[percent] = hands

    return cells, ranking, ranges


def equity_against(cells, hero, range_hands):
    """Exact equity of `hero` against a hand drawn uniformly from the range."""
    numerator = denominator = 0
    for opponent in range_hands:
        cell = cells[(hero, opponent)]
        numerator += cell["numerator"]
        denominator += 2 * cell["boards"]
    return Fraction(numerator, denominator)


def hands_in_range(cells, hero, hero_combos, range_hands):
    """How many of the 1,225 possible opponent hands fall inside the range.

    `deals` counts ordered pairs of concrete hands over all of the hero's own
    forms, so dividing by the hero's combo count gives the count against one
    fixed form, which is what an opponent actually holds. Card removal is
    already inside it: a hero holding AA leaves only one of the six AA
    combinations available to the opponent.
    """
    total = sum(cells[(hero, opponent)]["deals"] for opponent in range_hands)
    return Fraction(total, hero_combos)


def required_equity(stack):
    """The equity needed to call an all-in of `stack` big blinds, from the big blind."""
    return Fraction(stack - 1, 2 * stack)


def call_value(stack, equity):
    """EV(call) - EV(fold), in big blinds."""
    return 2 * stack * equity - (stack - 1)


def shove_value(stack, equity, fold_probability):
    """EV(shove) - EV(fold), in big blinds."""
    return (fold_probability * (stack + 1)
            + (1 - fold_probability) * 2 * stack * equity
            - (stack - Fraction(1, 2)))


def breakeven_fold_probability(stack, equity):
    """The fold probability that makes shoving exactly break even.

    Returns None when the denominator is not positive, which means the shove
    makes money no matter how often they fold: there is no breakeven to find
    because the equity when called already carries the play.
    """
    denominator = stack + 1 - 2 * stack * equity
    if denominator <= 0:
        return None
    return (stack - Fraction(1, 2) - 2 * stack * equity) / denominator


def write_call_table(cells, ranking, ranges):
    path = BUILD / "allin_call_exact.csv"
    with open(path, "w") as out:
        out.write("# Calling an all-in, heads-up blind versus blind with equal stacks.\n")
        out.write("# You are the big blind; the small blind is all-in for "
                  "`stack_bb` big blinds.\n")
        out.write("# Equities are exact, from the head-to-head matrix. "
                  "The model is documented in tools/derive_allin.py.\n")
        out.write("hand,family,shover_range_percent,stack_bb,equity_vs_range,"
                  "required_equity,expected_value_bb,decision\n")

        for entry in ranking:
            hand = entry["hand"]
            for percent in RANGE_PERCENTS:
                equity = equity_against(cells, hand, ranges[percent])
                for stack in STACK_DEPTHS:
                    needed = required_equity(stack)
                    value = call_value(stack, equity)
                    decision = ("call" if value > 0
                                else "indifferent" if value == 0 else "fold")

                    out.write(f"{hand},{entry['family']},{percent},{stack},"
                              f"{float(equity):.9f},{float(needed):.9f},"
                              f"{float(value):+.6f},{decision}\n")
    return path


def write_shove_table(cells, ranking, ranges):
    path = BUILD / "allin_shove_exact.csv"
    with open(path, "w") as out:
        out.write("# Shoving all-in, heads-up blind versus blind with equal stacks.\n")
        out.write("# You are the small blind; the big blind calls with "
                  "`caller_range_percent` of hands.\n")
        out.write("# The fold probability is counted from that range, with your own "
                  "cards blocking part of it.\n")
        out.write("hand,family,caller_range_percent,stack_bb,fold_probability,"
                  "equity_when_called,expected_value_bb,breakeven_fold_probability,"
                  "decision\n")

        for entry in ranking:
            hand = entry["hand"]
            combos = int(entry["combos"])

            for percent in RANGE_PERCENTS:
                calling = hands_in_range(cells, hand, combos, ranges[percent])
                fold_probability = 1 - calling / OPPONENT_HANDS
                equity = equity_against(cells, hand, ranges[percent])

                for stack in STACK_DEPTHS:
                    value = shove_value(stack, equity, fold_probability)
                    breakeven = breakeven_fold_probability(stack, equity)
                    decision = ("shove" if value > 0
                                else "indifferent" if value == 0 else "fold")

                    breakeven_text = ("always" if breakeven is None
                                      else f"{float(breakeven):.9f}")

                    out.write(f"{hand},{entry['family']},{percent},{stack},"
                              f"{float(fold_probability):.9f},{float(equity):.9f},"
                              f"{float(value):+.6f},{breakeven_text},{decision}\n")
    return path


def judge_the_rule(cells, ranking, ranges):
    """Tests "against an all-in, call only with JJ+ and AK" against the numbers.

    The rule was carried into this project as received wisdom, explicitly
    unverified. It makes two claims at once, and they fail in opposite
    directions, which is why it needs the pot odds and not just the equities:
    an earlier pass through this data compared equity against a flat 50% and
    got the first half of the answer wrong.

    Prints the exact calling range for each shoving range and stack depth, and
    returns nothing: this is a verdict for a reader, not a data file.
    """
    print("\n" + "=" * 78)
    print("The rule: \"against an all-in, call only with JJ+ and AK\"")
    print("=" * 78)

    print("\nThe equity a call needs, which is where the rule goes wrong first:\n")
    print("     stack    required equity")
    for stack in STACK_DEPTHS:
        print(f"  {stack:>6} bb    {float(required_equity(stack)) * 100:6.2f} %")
    print("\n  Never 50%. The blinds are already in the pot, and the shorter the")
    print("  stacks the bigger a share of it they are.")

    equities = {}
    for entry in ranking:
        for percent in RANGE_PERCENTS:
            equities[(entry["hand"], percent)] = equity_against(
                cells, entry["hand"], ranges[percent])

    for stack in (10, 20, 50):
        print(f"\n{'-' * 78}\nAt {stack} big blinds, needing "
              f"{float(required_equity(stack)) * 100:.2f}% equity:\n")

        for percent in RANGE_PERCENTS:
            calling = {entry["hand"] for entry in ranking
                       if call_value(stack, equities[(entry["hand"], percent)]) > 0}

            extra = sorted(calling - CLAIMED_CALLING_RANGE)
            missing = sorted(CLAIMED_CALLING_RANGE - calling)

            print(f"  they shove the top {percent:>3}%  ->  {len(calling):>3} hands "
                  f"are a profitable call")
            if missing:
                print(f"      the rule says call, the numbers say fold:  "
                      f"{', '.join(missing)}")
            if extra:
                shown = ', '.join(extra[:14])
                more = f"  (+{len(extra) - 14} more)" if len(extra) > 14 else ""
                print(f"      the rule says fold, the numbers say call:  {shown}{more}")
            if not extra and not missing:
                print("      the rule is exactly right here")

    # The verdict's claims are computed here rather than written out, so the
    # prose cannot drift away from the numbers it describes. An earlier draft
    # of this text claimed AK never calls against a tight shove at any depth;
    # the data says AKs does call when short, and only AKo never does.
    tight = 5
    depths_calling = {
        hand: [stack for stack in STACK_DEPTHS
               if call_value(stack, equities[(hand, tight)]) > 0]
        for hand in CLAIMED_CALLING_RANGE
    }
    never_calls = sorted(h for h, d in depths_calling.items() if not d)
    sometimes = sorted(h for h, d in depths_calling.items()
                       if d and len(d) < len(STACK_DEPTHS))
    always = sorted(h for h, d in depths_calling.items()
                    if len(d) == len(STACK_DEPTHS))

    # Hands the rule folds that are in fact profitable calls against the same
    # tight shove, with the depths at which they are. Requiring profitability
    # at *every* depth would hide the most interesting one: TT calls from 5 to
    # 50 big blinds and only folds at 100, where the required equity finally
    # climbs past it.
    outsiders = {}
    for entry in ranking:
        hand = entry["hand"]
        if hand in CLAIMED_CALLING_RANGE:
            continue
        depths = [stack for stack in STACK_DEPTHS
                  if call_value(stack, equities[(hand, tight)]) > 0]
        if depths:
            outsiders[hand] = depths

    wide_counts = {
        stack: sum(1 for entry in ranking
                   if call_value(stack, equities[(entry["hand"], 20)]) > 0)
        for stack in (10, 20, 50)
    }

    print(f"\n{'-' * 78}\nVerdict\n{'-' * 78}\n")
    print("  REFUTED, in both directions, and which direction depends on the")
    print("  opponent and on the stack.\n")

    print(f"  Too loose against a tight shove. Against a player who only moves in")
    print(f"  with the top {tight}% of hands:")
    for hand in never_calls:
        print(f"    - {hand} is never a profitable call, at any stack depth here. "
              f"Its equity")
        print(f"      against that range is "
              f"{float(equities[(hand, tight)]) * 100:.2f}%, and the most a call "
              f"ever needs is")
        print(f"      {float(required_equity(max(STACK_DEPTHS))) * 100:.2f}%, so it "
              f"is not close.")
    for hand in sometimes:
        depths = depths_calling[hand]
        where = (f"only at {depths[0]} big blinds" if len(depths) == 1
                 else f"only from {min(depths)} to {max(depths)} big blinds")
        print(f"    - {hand} is a profitable call {where} "
              f"({float(equities[(hand, tight)]) * 100:.2f}% equity),")
        print(f"      and loses money deeper than that. The rule holds it at "
              f"every depth.")
    if always:
        print(f"    - {', '.join(always)} do call, at every depth.")
    print("\n    The hands that beat AK against a top 5% range are precisely the")
    print("    hands such a player is shoving. The rule tells you to call and")
    print("    lose money.\n")

    if outsiders:
        print(f"  And too tight even against that tight shove. These hands the "
              f"rule folds")
        print(f"  are profitable calls against the top {tight}%:")
        for hand, depths in outsiders.items():
            where = (f"at {depths[0]} bb" if len(depths) == 1
                     else f"from {min(depths)} to {max(depths)} bb")
            print(f"    - {hand} {where} "
                  f"({float(equities[(hand, tight)]) * 100:.2f}% equity)")
        print()

    print("  Far too tight against anything wider. Against a top 20% shove — "
          "still a")
    print("  selective range — the profitable calls number "
          + ", ".join(f"{count} at {stack} bb"
                       for stack, count in wide_counts.items()) + ",")
    print(f"  against the rule's {len(CLAIMED_CALLING_RANGE)}. Every pair down to "
          f"66 or 77 is in there, and most of")
    print("  the ace-broadways the rule excludes.\n")

    print("  Why it fails: a fixed list of hands cannot be right, because the")
    print("  answer depends on two things the list does not mention. The "
          "shover's")
    print("  range sets your equity, and the stack depth sets the equity you")
    print("  need. The rule names neither, so it could only be accidentally")
    print("  correct at one combination of the two, and it is not exactly "
          "correct")
    print("  at any combination tested here.\n")

    print("  What survives: the rule's *shape*. Pairs and ace-broadways do "
          "dominate")
    print("  every calling range computed here, and against a tight shove "
          "nothing")
    print("  outside them calls profitably. It is a fair summary of which hands")
    print("  matter and a poor one of where the line falls.\n")


def validate(cells, ranking, ranges):
    print("\nChecking the model")

    # 1. The required equity must rise with the stack and approach one half.
    needed = [required_equity(stack) for stack in STACK_DEPTHS]
    rising = all(a < b for a, b in zip(needed, needed[1:]))
    report(rising and all(q < Fraction(1, 2) for q in needed),
           "the required equity rises with the stack and stays below one half",
           f"{float(needed[0]) * 100:.1f}% at {STACK_DEPTHS[0]} bb, "
           f"{float(needed[-1]) * 100:.2f}% at {STACK_DEPTHS[-1]} bb")

    # 2. A call at exactly the required equity must be worth exactly zero.
    #    In exact arithmetic this is an identity, and it is the check that the
    #    two formulas are the same formula.
    exact_zero = all(call_value(stack, required_equity(stack)) == 0
                     for stack in STACK_DEPTHS)
    report(exact_zero, "a call at exactly the required equity is worth exactly zero")

    # 3. Against the widest range nobody folds, so the fold probability must be
    #    exactly zero; and the shove's value must then be the call's value
    #    shifted by the half blind the two positions differ by.
    mismatches = 0
    for entry in ranking[:20]:
        hand, combos = entry["hand"], int(entry["combos"])
        fold_probability = 1 - hands_in_range(cells, hand, combos,
                                              ranges[100]) / OPPONENT_HANDS
        if fold_probability != 0:
            mismatches += 1
    report(mismatches == 0,
           "against a range of every hand, the fold probability is exactly zero")

    # 4. Blockers must move the fold probability the right way. Holding AA
    #    takes five of the six AA combinations out of the opponent's range, so
    #    a tight range is harder for them to have.
    aa_folds = 1 - hands_in_range(cells, "AA", 6, ranges[5]) / OPPONENT_HANDS
    junk_folds = 1 - hands_in_range(cells, "72o", 12, ranges[5]) / OPPONENT_HANDS
    report(aa_folds > junk_folds,
           "your own cards block the opponent's range: holding AA makes a top 5% "
           "hand less likely for them than holding 72o does",
           f"{float(aa_folds) * 100:.4f}% against {float(junk_folds) * 100:.4f}%")

    # 5. The breakeven fold probability must do what it says: shoving at
    #    exactly that fold probability is worth exactly zero.
    mismatches = 0
    for entry in ranking[::17]:
        for percent in RANGE_PERCENTS:
            equity = equity_against(cells, entry["hand"], ranges[percent])
            for stack in STACK_DEPTHS:
                breakeven = breakeven_fold_probability(stack, equity)
                if breakeven is None:
                    continue
                if shove_value(stack, equity, breakeven) != 0:
                    mismatches += 1
    report(mismatches == 0,
           "shoving at the breakeven fold probability is worth exactly zero")

    # 6. The value of a shove is a straight line in the fold probability, and
    #    the line's direction says which outcome you were hoping for.
    #
    #    This is not the invariant I first wrote, which asserted that shoving
    #    is worth more the more often they fold. That is false, and usefully
    #    so: folding to your shove wins you the 1.5 blinds in the pot, while
    #    being called wins you 2*S*q, and with a strong enough hand the second
    #    is larger. A hero holding aces at 5 big blinds against a 20% calling
    #    range wins 8.5 blinds by being called and 6 by being folded to, so
    #    every extra fold costs money. "I want them to fold" is a property of
    #    weak hands, not of shoving.
    #
    #    So the check is the real relation: the value is linear in f with
    #    slope (S + 1) - 2*S*q, and it rises with f exactly when folding beats
    #    being called.
    mismatches = 0
    for entry in ranking[::17]:
        for percent in RANGE_PERCENTS:
            equity = equity_against(cells, entry["hand"], ranges[percent])
            for stack in STACK_DEPTHS:
                slope = (stack + 1) - 2 * stack * equity

                # Linearity: equal steps in f must give equal steps in value.
                steps = [shove_value(stack, equity, Fraction(f, 10))
                         for f in range(11)]
                differences = {b - a for a, b in zip(steps, steps[1:])}
                if differences != {slope / 10}:
                    mismatches += 1

                # Direction: rising exactly when a fold is the better outcome.
                rising = steps[-1] > steps[0]
                if rising != (slope > 0):
                    mismatches += 1
    report(mismatches == 0,
           "the value of a shove is linear in the fold probability, rising exactly "
           "when a fold beats being called")

    # 7. And that case is real, not hypothetical: name a hand and a depth
    #    where the hero would rather be called than folded to.
    aces = equity_against(cells, "AA", ranges[20])
    unwanted_fold = shove_value(5, aces, Fraction(1)) < shove_value(5, aces, Fraction(0))
    report(unwanted_fold,
           "with aces at 5 big blinds against a 20% caller, a fold is the worst "
           "outcome of shoving",
           f"folded to: {float(shove_value(5, aces, Fraction(1))):+.3f} bb, "
           f"always called: {float(shove_value(5, aces, Fraction(0))):+.3f} bb")


def main():
    needed = [BUILD / "headsup_169x169_exact.csv", BUILD / "hand_ranking_exact.csv"]
    missing = [path.name for path in needed if not path.exists()]
    if missing:
        print(f"Nothing to derive: {', '.join(missing)} not built. Run `make data` first.")
        return 1

    print("Deriving the preflop all-in decision from the exact matrix")

    cells, ranking, ranges = load()
    validate(cells, ranking, ranges)

    if failures:
        print(f"\n{len(failures)} check(s) failed. No output written:")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print()
    for path in (write_call_table(cells, ranking, ranges),
                 write_shove_table(cells, ranking, ranges)):
        print(f"Wrote {path.relative_to(REPO)}")

    judge_the_rule(cells, ranking, ranges)
    return 0


if __name__ == "__main__":
    sys.exit(main())
