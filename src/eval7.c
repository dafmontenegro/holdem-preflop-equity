/* eval7.c — implementation of the hand evaluator described in eval7.h. */

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

int hand_score(const int *cards, int count)
{
    /* Three summaries of the cards, each answering one kind of question:
     *
     *   rank_count[r]   how many cards of rank r, which finds pairs and trips
     *   suit_count[s]   how many cards of suit s, which finds a flush
     *   rank_mask       which ranks are present at all, which finds a straight
     *   suit_mask[s]    which ranks are present within suit s, which finds a
     *                   straight flush and the five cards of a flush
     */
    int rank_count[NUM_RANKS] = { 0 };
    int suit_count[NUM_SUITS] = { 0 };
    int suit_mask[NUM_SUITS]  = { 0 };
    int rank_mask = 0;

    for (int i = 0; i < count; i++) {
        int rank = RANK_OF(cards[i]);
        int suit = SUIT_OF(cards[i]);

        rank_count[rank]++;
        suit_count[suit]++;
        suit_mask[suit] |= 1 << rank;
        rank_mask       |= 1 << rank;
    }

    /* The flush suit, if there is one.
     *
     * With seven cards or fewer there can be at most one suit holding five
     * cards, since two such suits would need ten. So a single index is enough
     * and no "best of several flushes" comparison is needed. */
    int flush_suit = -1;
    for (int suit = 0; suit < NUM_SUITS; suit++) {
        if (suit_count[suit] >= 5)
            flush_suit = suit;
    }

    /* Collect the repeated ranks in one downward pass, so that whatever is
     * found first is the highest of its kind. Seven cards allow at most one
     * quad, two trips (3+3+1) and three pairs (2+2+2+1), which is why there
     * are exactly these slots and no more. */
    int quad = -1;
    int trips_high = -1, trips_low = -1;
    int pair_high = -1, pair_mid = -1, pair_low = -1;

    for (int rank = RANK_A; rank >= RANK_2; rank--) {
        switch (rank_count[rank]) {
        case 4:
            quad = rank;
            break;
        case 3:
            if (trips_high < 0)      trips_high = rank;
            else if (trips_low < 0)  trips_low  = rank;
            break;
        case 2:
            if (pair_high < 0)       pair_high = rank;
            else if (pair_mid < 0)   pair_mid  = rank;
            else if (pair_low < 0)   pair_low  = rank;
            break;
        default:
            break;
        }
    }
    (void)pair_low;  /* Found for completeness; a third pair never scores, see
                      * the two pair case below, where it can only matter as a
                      * kicker and is picked up there as an ordinary rank. */

    /* The categories are tested from strongest to weakest, and the first match
     * wins. The order is what makes the result the *best* five-card hand: a
     * player holding both a flush and a pair has a flush, and asking about the
     * flush first is how that comes out right. */

    /* 1. Straight flush: five consecutive ranks inside the flush suit.
     *    Checked on the flush suit's own mask, not the overall rank mask, so
     *    that five ranks spread across suits cannot be mistaken for one. */
    if (flush_suit >= 0) {
        int top = highest_straight(suit_mask[flush_suit]);
        if (top >= 0)
            return SCORE(CAT_STRAIGHT_FLUSH, top, 0, 0, 0, 0);
    }

    /* 2. Four of a kind, plus the best remaining card as a kicker.
     *    The kicker is the highest rank other than the quad, whatever its
     *    count: with 7-7-7-7-K-K-2 the kicker is the king, even though the
     *    king is itself a pair. Only one card of it plays. */
    if (quad >= 0) {
        int kicker = -1;
        for (int rank = RANK_A; rank >= RANK_2; rank--) {
            if (rank != quad && rank_count[rank] > 0) {
                kicker = rank;
                break;
            }
        }
        return SCORE(CAT_FOUR_OF_KIND, quad, kicker, 0, 0, 0);
    }

    /* 3. Full house: the best trips, then the best pair to go with it.
     *    Two subtleties, both of which the tests pin down:
     *
     *    - With two trips, the lower trips plays as the pair. Holding
     *      K-K-K-5-5-5-2 the hand is kings full of fives.
     *    - The pair is whichever candidate is higher: the second trips or an
     *      actual pair. With seven cards or fewer only one of the two can be
     *      present, since two trips and a pair would need eight cards, so in
     *      Hold'em this is never a real contest. Taking the higher of the two
     *      costs nothing and keeps the function correct for larger hands. */
    if (trips_high >= 0 && (trips_low >= 0 || pair_high >= 0)) {
        int pair = trips_low > pair_high ? trips_low : pair_high;
        return SCORE(CAT_FULL_HOUSE, trips_high, pair, 0, 0, 0);
    }

    /* 4. Flush: the five highest cards of the flush suit.
     *    All five ranks go into the score, because two flushes are compared
     *    card by card all the way down. */
    if (flush_suit >= 0) {
        int top5[5];
        int found = 0;
        for (int rank = RANK_A; rank >= RANK_2 && found < 5; rank--) {
            if ((suit_mask[flush_suit] >> rank) & 1)
                top5[found++] = rank;
        }
        return SCORE(CAT_FLUSH, top5[0], top5[1], top5[2], top5[3], top5[4]);
    }

    /* 5. Straight: five consecutive ranks in any suits. */
    int straight_top = highest_straight(rank_mask);
    if (straight_top >= 0)
        return SCORE(CAT_STRAIGHT, straight_top, 0, 0, 0, 0);

    /* 6. Three of a kind, plus the two best remaining cards.
     *    Reaching here means there is no second trips and no pair, so every
     *    other rank is a single card. */
    if (trips_high >= 0) {
        int kickers[2];
        int found = 0;
        for (int rank = RANK_A; rank >= RANK_2 && found < 2; rank--) {
            if (rank != trips_high && rank_count[rank] > 0)
                kickers[found++] = rank;
        }
        return SCORE(CAT_THREE_OF_KIND, trips_high, kickers[0], kickers[1], 0, 0);
    }

    /* 7. Two pair: the two highest pairs, plus one kicker.
     *    The kicker is the highest rank outside those two pairs, and with
     *    three pairs that can be the third pair itself. Holding
     *    9-9-7-7-5-5-2 the hand is nines and sevens with a five kicker,
     *    because the five outranks the deuce. Looking for "the highest rank
     *    that is not one of the two pairs" gets this right without treating
     *    the third pair as a special case. */
    if (pair_mid >= 0) {
        int kicker = -1;
        for (int rank = RANK_A; rank >= RANK_2; rank--) {
            if (rank != pair_high && rank != pair_mid && rank_count[rank] > 0) {
                kicker = rank;
                break;
            }
        }
        return SCORE(CAT_TWO_PAIR, pair_high, pair_mid, kicker, 0, 0);
    }

    /* 8. One pair, plus the three best remaining cards. */
    if (pair_high >= 0) {
        int kickers[3];
        int found = 0;
        for (int rank = RANK_A; rank >= RANK_2 && found < 3; rank--) {
            if (rank != pair_high && rank_count[rank] > 0)
                kickers[found++] = rank;
        }
        return SCORE(CAT_PAIR, pair_high, kickers[0], kickers[1], kickers[2], 0);
    }

    /* 9. High card: the five highest ranks. */
    int top5[5];
    int found = 0;
    for (int rank = RANK_A; rank >= RANK_2 && found < 5; rank--) {
        if (rank_count[rank] > 0)
            top5[found++] = rank;
    }
    return SCORE(CAT_HIGH_CARD, top5[0], top5[1], top5[2], top5[3], top5[4]);
}
