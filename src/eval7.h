/* eval7.h — the hand evaluator: turns 5, 6 or 7 cards into one comparable number.
 *
 * WHAT IT ANSWERS
 * ---------------
 * Given a player's cards, `hand_score` returns an integer with one property,
 * and that property is the whole point:
 *
 *     score(A) > score(B)  exactly when hand A beats hand B
 *     score(A) == score(B) exactly when the two hands split the pot
 *
 * In Hold'em a player makes the best five-card hand out of seven (two private
 * cards plus five on the board), and may use two, one or none of their own.
 * The evaluator does not search those 21 five-card subsets: it reads the whole
 * set of cards at once and builds the best hand directly, which is both faster
 * and easier to argue is correct.
 *
 * It accepts 5, 6 or 7 cards so that the same function can score a finished
 * Hold'em hand (7), a board on its own (5), and every five-card hand in the
 * deck (5) when the tests check the evaluator against the known hand counts.
 *
 * THE SCORE LAYOUT
 * ----------------
 * The score packs a category and up to five ranks into one integer, so that
 * comparing two hands is a single integer comparison:
 *
 *     bits 20-23   category   0 = high card ... 8 = straight flush
 *     bits 16-19   rank 1     the most significant rank of the hand
 *     bits 12-15   rank 2
 *     bits  8-11   rank 3
 *     bits  4-7    rank 4
 *     bits  0-3    rank 5     the least significant rank
 *
 * Each field is four bits, which is exactly enough for a rank 0..12, and the
 * fields are ordered from most to least significant tiebreak. That is what
 * makes a plain `>` do the right thing: the category dominates, and within a
 * category the ranks are compared in the order a dealer would compare them.
 *
 * Which ranks go into the slots depends on the category, and the five slots
 * are *not* simply the five cards sorted. They are the ranks a dealer reads
 * out, in the order that settles the hand:
 *
 *     straight flush   top card of the straight                 (1 slot used)
 *     four of a kind   the quad rank, then the kicker           (2 slots)
 *     full house       the trips rank, then the pair rank       (2 slots)
 *     flush            the five flush card ranks, high to low   (5 slots)
 *     straight         top card of the straight                 (1 slot)
 *     three of a kind  the trips rank, then two kickers         (3 slots)
 *     two pair         high pair, low pair, then one kicker     (3 slots)
 *     one pair         the pair rank, then three kickers        (4 slots)
 *     high card        the five card ranks, high to low         (5 slots)
 *
 * Unused slots are left at zero. That is safe because two hands are only ever
 * compared slot by slot when they share a category, and within a category the
 * same slots are always filled.
 *
 * A straight is identified by its top card alone, which is enough: two
 * straights with the same top card are the same five ranks. The ace-low
 * straight A-2-3-4-5 ("the wheel") is the one case where the ace is not the
 * top card, and it is scored as a five-high straight, so it loses to 2-3-4-5-6.
 *
 * WHAT A SCORE IS NOT
 * -------------------
 * The numeric distance between two scores means nothing. A score is an
 * ordering device, not a measure of how much better one hand is. Nothing in
 * the engine may subtract or average scores; the only legal operations are
 * `<`, `>` and `==`.
 */
#ifndef EVAL7_H
#define EVAL7_H

/* Hand categories, weakest to strongest. The numbering is the one used by the
 * exploration phase and by the published tables, so the two can be compared. */
enum hand_category {
    CAT_HIGH_CARD      = 0,
    CAT_PAIR           = 1,
    CAT_TWO_PAIR       = 2,
    CAT_THREE_OF_KIND  = 3,
    CAT_STRAIGHT       = 4,
    CAT_FLUSH          = 5,
    CAT_FULL_HOUSE     = 6,
    CAT_FOUR_OF_KIND   = 7,
    CAT_STRAIGHT_FLUSH = 8,
    NUM_CATEGORIES     = 9
};

/* Human-readable category names, indexed by `enum hand_category`. */
extern const char *const CATEGORY_NAMES[NUM_CATEGORIES];

/* Scores a hand of `count` cards, where count is 5, 6 or 7.
 *
 * The cards must be legal and distinct; the caller is responsible for that
 * (see cards_are_distinct). Passing a duplicate does not crash, it silently
 * returns the score of a hand that cannot be dealt.
 */
int hand_score(const int *cards, int count);

/* The category of a scored hand, i.e. the top four bits pulled back out. */
static inline int score_category(int score) { return score >> 20; }

#endif /* EVAL7_H */
