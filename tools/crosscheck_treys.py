"""Checks individual matchups against treys, an evaluator written by someone else.

WHY THIS IS A DIFFERENT KIND OF CHECK
-------------------------------------
Everything else that validates this engine is either internal (the evaluator's
test suite, the invariants each generator checks before writing) or a
comparison against published summary figures. Both leave one gap: a bug that
is consistent with itself and that no published table happens to cover.

This closes it. `treys` is an independent poker hand evaluator, written by
other people in another language, installed from PyPI. For a handful of chosen
matchups this script enumerates all 1,712,304 boards with treys and compares
**wins, ties and losses as exact integers** against what this engine wrote.
Not the equity, which could match by luck while the three counts are wrong;
the counts themselves.

Each matchup takes about half a minute, so this is a spot check by design, not
a sweep. The matchups are chosen to exercise what is most likely to be wrong
rather than what is most likely to be right:

  - A dominated hand against the hand dominating it, where the kicker logic
    decides nearly every board.
  - The same pair of hand types in two different suit arrangements, which is
    where a mistake in the suit isomorphism would show up as two cells that
    should differ but do not, or vice versa.
  - A matchup turning on flushes, and one turning on straights, including the
    wheel.
  - A pair against two overcards, the classic coin flip, where ties are
    common and a tie counted as a win would pass an equity check at this
    precision but not a count check.

This is also the only part of the project that needs anything installed:
`pip install -r requirements-dev.txt`.
"""

import csv
import sys
from itertools import combinations
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUILD = REPO / "build"

# Each entry is (hero cards, opponent cards, what the matchup is there to test).
# The hero's cards must be the representative this engine uses for that hand
# type: the high card in clubs, and the low card in clubs if suited or in
# diamonds if not.
MATCHUPS = [
    ("AcKd", "AhAs", "a dominated hand: pairing the king loses to the pair of aces"),
    ("AcKd", "AdAh", "the same two hand types, different suits: the opponent's "
                     "diamond ace caps the hero's king-high diamond flush"),
    ("AcKd", "KhKs", "the hero's ace is live but his king is nearly dead"),
    ("AcAd", "KcQc", "a pair against two suited overcards, where flushes and "
                     "straights are the opponent's whole case"),
    ("9c8c", "AhAs", "suited connectors against aces: straights and flushes "
                     "against a made hand"),
    ("Ac5c", "KdQh", "the wheel matters here, and the ace is both a high card "
                     "and the bottom of a straight"),
    ("7c7d", "AcKd", "the classic coin flip, where ties are common enough to "
                     "catch a tie miscounted as a win"),
]

failures = []


def read_engine_results():
    """Loads the engine's per-matchup counts, keyed by (hero label, opponent cards)."""
    path = BUILD / "headsup_169_vs_1225_exact.csv"
    if not path.exists():
        return None

    results = {}
    with open(path) as handle:
        for row in csv.DictReader(line for line in handle if not line.startswith("#")):
            results[(row["hero"], row["opponent_cards"])] = (
                int(row["wins"]), int(row["ties"]), int(row["losses"])
            )
    return results


RANKS = "23456789TJQKA"
SUITS = "cdhs"


def card_number(card):
    """The engine's card encoding: rank * 4 + suit, with ranks from the deuce."""
    return RANKS.index(card[0]) * 4 + SUITS.index(card[1])


def card_text(number):
    return RANKS[number // 4] + SUITS[number % 4]


def engine_opponent_text(first, second):
    """Two cards in the order the engine writes them: ascending card number."""
    low, high = sorted([card_number(first), card_number(second)])
    return card_text(low) + card_text(high)


def orbit_members(hero_cards, opponent_cards):
    """Every opponent hand equivalent to this one, as the engine would write it.

    The engine stores one row per *orbit*: opponent hands that renaming the
    suits maps onto each other, while leaving the hero's own hand alone, have
    identical counts against the hero and share a row. So looking a matchup up
    means asking for any member of its orbit.

    The renamings that leave the hero alone are the hero's stabiliser, found
    here the same way src/suits.h finds it: by trying all 24 renamings of the
    four suits and keeping those that map the hero's two cards back onto the
    same two cards.

    Returning every member rather than one canonical choice also means the
    comparison tests something extra: that the orbit really is an orbit. If
    two hands in it had different counts, the engine's single row could not
    match both, and this script enumerates the member it was given rather than
    the member the engine stored.
    """
    from itertools import permutations

    hero = [hero_cards[0:2], hero_cards[2:4]]
    opponent = [opponent_cards[0:2], opponent_cards[2:4]]
    hero_set = {card_number(card) for card in hero}

    members = set()
    for renaming in permutations(range(4)):
        def rename(card):
            return RANKS[RANKS.index(card[0])] + SUITS[renaming[SUITS.index(card[1])]]

        if {card_number(rename(card)) for card in hero} != hero_set:
            continue    # this renaming moves the hero's hand, so it is not allowed

        members.add(engine_opponent_text(rename(opponent[0]), rename(opponent[1])))

    return members


def hand_label(cards):
    """The hand type label for two cards, e.g. "AcKd" -> "AKo"."""
    ranks = "23456789TJQKA"
    first_rank, first_suit = cards[0], cards[1]
    second_rank, second_suit = cards[2], cards[3]

    high, low = sorted([first_rank, second_rank], key=ranks.index, reverse=True)
    if first_rank == second_rank:
        return high + low
    return high + low + ("s" if first_suit == second_suit else "o")


def enumerate_with_treys(hero_cards, opponent_cards):
    """Counts wins, ties and losses over all 1,712,304 boards, using treys."""
    from treys import Card, Evaluator

    evaluator = Evaluator()
    hero = [Card.new(hero_cards[0:2]), Card.new(hero_cards[2:4])]
    opponent = [Card.new(opponent_cards[0:2]), Card.new(opponent_cards[2:4])]

    used = set(hero + opponent)
    deck = [Card.new(rank + suit) for rank in "23456789TJQKA" for suit in "cdhs"]
    deck = [card for card in deck if card not in used]
    assert len(deck) == 48, "the four known cards must be distinct"

    wins = ties = losses = 0
    for board in combinations(deck, 5):
        cards = list(board)
        # treys ranks hands with lower numbers being better.
        hero_rank = evaluator.evaluate(cards, hero)
        opponent_rank = evaluator.evaluate(cards, opponent)

        if hero_rank < opponent_rank:
            wins += 1
        elif hero_rank == opponent_rank:
            ties += 1
        else:
            losses += 1

    return wins, ties, losses


def main():
    try:
        import treys  # noqa: F401
    except ImportError:
        print("treys is not installed. Run: pip install -r requirements-dev.txt")
        return 1

    engine = read_engine_results()
    if engine is None:
        print("Nothing to check: build/headsup_169_vs_1225_exact.csv not built. "
              "Run `make data` first.")
        return 1

    print("Cross-checking individual matchups against treys")
    print("================================================")
    print(f"\nEach matchup enumerates all 1,712,304 boards twice over, once per "
          f"evaluator.\n")

    for hero_cards, opponent_cards, reason in MATCHUPS:
        label = hand_label(hero_cards)

        stored = None
        for member in orbit_members(hero_cards, opponent_cards):
            if (label, member) in engine:
                stored = engine[(label, member)]
                break

        if stored is None:
            print(f"  FAIL  {label} ({hero_cards}) vs {opponent_cards}: "
                  f"the engine has no row for this matchup or any equivalent to it")
            failures.append(f"{label} vs {opponent_cards} missing")
            continue

        theirs = enumerate_with_treys(hero_cards, opponent_cards)
        ours = stored

        matched = theirs == ours
        print(f"  {'ok  ' if matched else 'FAIL'}  {label} ({hero_cards}) vs "
              f"{opponent_cards}: {ours[0]} wins, {ours[1]} ties, {ours[2]} losses"
              f"   equity {(2 * ours[0] + ours[1]) / (2 * sum(ours)) * 100:.4f} %")
        print(f"        {reason}")
        if not matched:
            print(f"        treys says {theirs[0]} wins, {theirs[1]} ties, "
                  f"{theirs[2]} losses")
            failures.append(f"{label} vs {opponent_cards}")

    if failures:
        print(f"\n{len(failures)} matchup(s) disagree with treys:")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print(f"\nAll {len(MATCHUPS)} matchups agree with treys on every count.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
