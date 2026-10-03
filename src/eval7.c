/* eval7.c — implementation of the hand evaluator described in eval7.h. */

#include <string.h>   /* memset, to clear a summary */

#include "eval7.h"
#include "cards.h"

const char *const CATEGORY_NAMES[NUM_CATEGORIES] = {
    "high card", "pair", "two pair", "three of a kind", "straight",
    "flush", "full house", "four of a kind", "straight flush"
};

/* Packs a category and its tiebreak ranks into a score. See eval7.h for the
 * bit layout and for which ranks belong in which slot. */
#define SCORE(category, r1, r2, r3, r4, r5) \
    (((category) << 20) | ((r1) << 16) | ((r2) << 12) | ((r3) << 8) | ((r4) << 4) | (r5))

/* Finds the highest straight in a 13-bit rank mask, where bit r is set when at
 * least one card of rank r is present.
 *
 * Returns the rank of the straight's top card, or -1 when there is no straight.
 *
 * HOW THE BIT TRICK WORKS
 * -----------------------
 * First the mask is shifted up by one and an extra bit is added at the bottom
 * when an ace is present:
 *
 *     extended = (mask << 1) | (ace present ? 1 : 0)
 *
 * The shift makes room for that bottom bit, which is the ace playing low. In
 * `extended`, bit (r+1) means "a card of rank r is present" and bit 0 means
 * "an ace is available below the deuce". This is what lets A-2-3-4-5 be found
 * by the same test as every other straight, instead of as a special case.
 *
 * Then five copies of `extended`, each shifted one place further, are ANDed:
 *
 *     runs = extended & (extended>>1) & (extended>>2) & (extended>>3) & (extended>>4)
 *
 * A bit p survives only if bits p, p+1, p+2, p+3 and p+4 were all set, that
 * is, only if five consecutive ranks are present. Any surviving bit marks a
 * straight; the highest one marks the highest straight.
 *
 * Reading the answer back: a surviving bit p stands for the five ranks
 * (p-1) through (p+3), so the top card of the straight has rank p+3.
 *
 * Worked example, the wheel. Holding A-2-3-4-5 the mask has bits 12 (ace) and
 * 0,1,2,3 (deuce through five). Shifting up and setting the ace-low bit gives
 * `extended` bits 0,1,2,3,4 and 13. Bit 0 survives the AND, because bits 0
 * through 4 are all set, so the top card has rank 0+3 = 3, the five. The
 * wheel is scored as a five-high straight, which is why it loses to
 * 2-3-4-5-6, exactly as the rules say.
 */
static int highest_straight(int rank_mask)
{
    int ace_low  = (rank_mask >> RANK_A) & 1;
    int extended = (rank_mask << 1) | ace_low;

    int runs = extended
             & (extended >> 1)
             & (extended >> 2)
             & (extended >> 3)
             & (extended >> 4);

    if (runs == 0)
        return -1;

    int highest_bit = 31 - __builtin_clz((unsigned)runs);
    return highest_bit + 3;
}

void card_summary_init(struct card_summary *summary)
{
    memset(summary, 0, sizeof *summary);
}

/* Adds one card, walking one step down the ladder of nested rank masks.
 *
 * The first time a rank is seen it joins `present`; the second time it joins
 * `two_plus`, and so on. Each `if` is therefore asking "have I seen this rank
 * this many times already?", and the first one that says no is where the card
 * lands. A rank seen a fifth time is impossible with one deck, so there is no
 * branch for it.
 */
void card_summary_add(struct card_summary *summary, int card)
{
    int rank = RANK_OF(card);
    int suit = SUIT_OF(card);
    int bit  = 1 << rank;

    if (!(summary->present & bit))         summary->present    |= bit;
    else if (!(summary->two_plus & bit))   summary->two_plus   |= bit;
    else if (!(summary->three_plus & bit)) summary->three_plus |= bit;
    else                                   summary->four       |= bit;

    summary->suit_count[suit]++;
    summary->suit_mask[suit] |= bit;
}

void card_summary_add_cards(struct card_summary *summary, const int *cards, int count)
{
    for (int i = 0; i < count; i++)
        card_summary_add(summary, cards[i]);
}

int hand_score(const int *cards, int count)
{
    struct card_summary summary;

    card_summary_init(&summary);
    card_summary_add_cards(&summary, cards, count);

    return score_summary(&summary);
}

/* The highest rank in a mask, or -1 if the mask is empty.
 *
 * `__builtin_clz` counts the leading zero bits of a 32-bit word, so
 * 31 minus that count is the position of the highest set bit. One
 * instruction on every machine this runs on, in place of a loop over ranks. */
static inline int top_rank(int mask)
{
    return mask != 0 ? 31 - __builtin_clz((unsigned)mask) : -1;
}

/* Writes the highest `wanted` ranks of a mask into `out`, highest first.
 *
 * Used for the five cards of a flush or a high-card hand, and for kickers.
 * Each step takes the top rank and clears it, so the loop runs once per rank
 * actually needed instead of once per rank in the deck. */
static inline void top_ranks(int mask, int wanted, int *out)
{
    for (int i = 0; i < wanted; i++) {
        int rank = top_rank(mask);
        out[i] = rank;
        mask &= ~(1 << rank);
    }
}

int score_summary(const struct card_summary *summary)
{
    /* The flush suit, if there is one.
     *
     * With seven cards or fewer there can be at most one suit holding five
     * cards, since two such suits would need ten. So a single index is enough
     * and no "best of several flushes" comparison is needed. */
    int flush_suit = -1;
    for (int suit = 0; suit < NUM_SUITS; suit++) {
        if (summary->suit_count[suit] >= 5)
            flush_suit = suit;
    }

    /* The nested masks are turned into exact ones: a rank held exactly twice
     * is in `two_plus` but not in `three_plus`. After this, each mask holds
     * only the ranks of that multiplicity, and the highest of each is one bit
     * scan away. */
    int quads          = summary->four;
    int exactly_trips  = summary->three_plus & ~summary->four;
    int exactly_pairs  = summary->two_plus & ~summary->three_plus;

    /* The categories are tested from strongest to weakest, and the first match
     * wins. The order is what makes the result the *best* five-card hand: a
     * player holding both a flush and a pair has a flush, and asking about the
     * flush first is how that comes out right. */

    /* 1. Straight flush: five consecutive ranks inside the flush suit.
     *    Checked on the flush suit's own mask, not the overall rank mask, so
     *    that five ranks spread across suits cannot be mistaken for one. */
    if (flush_suit >= 0) {
        int top = highest_straight(summary->suit_mask[flush_suit]);
        if (top >= 0)
            return SCORE(CAT_STRAIGHT_FLUSH, top, 0, 0, 0, 0);
    }

    /* 2. Four of a kind, plus the best remaining card as a kicker.
     *    The kicker is the highest rank other than the quad, whatever its
     *    multiplicity: with 7-7-7-7-K-K-2 the kicker is the king, even though
     *    the king is itself a pair. Only one card of it plays. */
    if (quads != 0) {
        int quad   = top_rank(quads);
        int kicker = top_rank(summary->present & ~(1 << quad));

        return SCORE(CAT_FOUR_OF_KIND, quad, kicker, 0, 0, 0);
    }

    /* 3. Full house: the best trips, then the best pair to go with it.
     *
     *    With two trips the lower trips plays as the pair, so K-K-K-5-5-5-2 is
     *    kings full of fives. The pair taken is the higher of the second trips
     *    and the best actual pair; with seven cards or fewer only one of the
     *    two can be present, since two trips and a pair would need eight
     *    cards, so in Hold'em this is never a real contest. Taking the higher
     *    costs nothing and keeps the function correct for larger hands. */
    if (exactly_trips != 0) {
        int trips_high = top_rank(exactly_trips);
        int other_pair = top_rank((exactly_trips & ~(1 << trips_high)) | exactly_pairs);

        if (other_pair >= 0)
            return SCORE(CAT_FULL_HOUSE, trips_high, other_pair, 0, 0, 0);
    }

    /* 4. Flush: the five highest cards of the flush suit.
     *    All five ranks go into the score, because two flushes are compared
     *    card by card all the way down. */
    if (flush_suit >= 0) {
        int top5[5];
        top_ranks(summary->suit_mask[flush_suit], 5, top5);

        return SCORE(CAT_FLUSH, top5[0], top5[1], top5[2], top5[3], top5[4]);
    }

    /* 5. Straight: five consecutive ranks in any suits. */
    int straight_top = highest_straight(summary->present);
    if (straight_top >= 0)
        return SCORE(CAT_STRAIGHT, straight_top, 0, 0, 0, 0);

    /* 6. Three of a kind, plus the two best remaining cards.
     *    Reaching here means there is no second trips and no pair, so every
     *    other rank is a single card. */
    if (exactly_trips != 0) {
        int trips = top_rank(exactly_trips);
        int kickers[2];
        top_ranks(summary->present & ~(1 << trips), 2, kickers);

        return SCORE(CAT_THREE_OF_KIND, trips, kickers[0], kickers[1], 0, 0);
    }

    /* 7. Two pair: the two highest pairs, plus one kicker.
     *    The kicker is the highest rank outside those two pairs, and with
     *    three pairs that can be the third pair itself: 9-9-7-7-5-5-2 is nines
     *    and sevens with a five kicker, because the five outranks the deuce.
     *    Masking out only the two pairs that play gets this right without
     *    treating the third pair as a special case. */
    int pair_high = top_rank(exactly_pairs);
    if (pair_high >= 0) {
        int pair_mid = top_rank(exactly_pairs & ~(1 << pair_high));

        if (pair_mid >= 0) {
            int kicker = top_rank(summary->present
                                  & ~(1 << pair_high) & ~(1 << pair_mid));

            return SCORE(CAT_TWO_PAIR, pair_high, pair_mid, kicker, 0, 0);
        }

        /* 8. One pair, plus the three best remaining cards. */
        int kickers[3];
        top_ranks(summary->present & ~(1 << pair_high), 3, kickers);

        return SCORE(CAT_PAIR, pair_high, kickers[0], kickers[1], kickers[2], 0);
    }

    /* 9. High card: the five highest ranks. */
    int top5[5];
    top_ranks(summary->present, 5, top5);

    return SCORE(CAT_HIGH_CARD, top5[0], top5[1], top5[2], top5[3], top5[4]);
}
