/* hands169.h — the 169 starting hand types, and why there are 169 of them.
 *
 * There are C(52,2) = 1,326 distinct two-card starting hands. Most of them
 * are strategically identical, because suits have no order in Hold'em: 9c8c
 * and 9h8h cannot be told apart by anything that matters, since renaming the
 * suits turns any board for one into an equally likely board for the other.
 * What survives that relabelling is only the two ranks and whether they share
 * a suit, which leaves three families:
 *
 *     pairs     AA  down to  22      13 types, C(4,2) =  6 combos each =  78
 *     suited    AKs down to  32s      78 types,            4 combos each = 312
 *     offsuit   AKo down to  32o      78 types,       4 x 3 = 12 combos each = 936
 *                                    ---                                   -----
 *                                    169                                   1,326
 *
 * The 78 comes from C(13,2), the ways to pick two different ranks. The combo
 * counts are what each type is worth when a result has to be averaged back
 * over all 1,326 concrete hands: a weighted average over the 169 types with
 * these weights is the same as an average over the 1,326 hands. Several of
 * the engine's checks depend on exactly that, so `combos` is carried in every
 * data file rather than left for the reader to look up.
 *
 * REPRESENTATIVE HANDS
 * --------------------
 * Each type is computed from one representative pair of cards, which is legal
 * precisely because of the suit relabelling argument above. The choice is
 * fixed and arbitrary: the high card is always a club, and the low card is a
 * club when the hand is suited and a diamond when it is not.
 *
 * ORDER
 * -----
 * Types come out high rank first, then low rank, offsuit before suited. That
 * is the order used by the exploration phase, so a reproduction can be diffed
 * line by line against exploration/data/.
 */
#ifndef HANDS169_H
#define HANDS169_H

#include <stdio.h>
#include <stdlib.h>   /* exit, for the aborts below */

#include "cards.h"

#define NUM_HAND_TYPES 169

struct hand_type {
    int  high_rank;     /* the higher of the two ranks, or either one for a pair */
    int  low_rank;      /* the lower of the two ranks, equal to high_rank for a pair */
    bool suited;        /* false for pairs, which can never be suited */
    int  combos;        /* 6 for a pair, 4 for suited, 12 for offsuit */
    int  cards[2];      /* the representative two cards */
    char label[4];      /* "AA", "AKs", "AKo" */
    const char *family; /* "pair", "suited" or "offsuit" */
};

/* Fills `out` with all 169 types in the order described above.
 * Returns the number written, which is always NUM_HAND_TYPES. */
static inline int hand_types_all(struct hand_type *out)
{
    int count = 0;

    for (int high = RANK_A; high >= RANK_2; high--) {
        for (int low = high; low >= RANK_2; low--) {
            /* suited = 0 is offsuit and comes first; a pair has only one form,
             * so the suited pass is skipped for it. */
            for (int suited = 0; suited < 2; suited++) {
                bool is_pair = (high == low);
                if (is_pair && suited)
                    continue;

                struct hand_type *type = &out[count++];

                type->high_rank = high;
                type->low_rank  = low;
                type->suited    = suited != 0;
                type->combos    = is_pair ? 6 : (suited ? 4 : 12);
                type->family    = is_pair ? "pair" : (suited ? "suited" : "offsuit");

                /* The representative cards. For a pair the two suits must
                 * differ, so clubs and diamonds are used. */
                type->cards[0] = CARD_OF(high, SUIT_CLUBS);
                type->cards[1] = CARD_OF(low, suited ? SUIT_CLUBS : SUIT_DIAMONDS);

                type->label[0] = RANK_CHARS[high];
                type->label[1] = RANK_CHARS[low];
                if (is_pair) {
                    type->label[2] = '\0';
                } else {
                    type->label[2] = suited ? 's' : 'o';
                    type->label[3] = '\0';
                }
            }
        }
    }
    return count;
}

/* The index of the hand type two concrete cards belong to.
 *
 * Aborts on a hand that cannot exist, rather than returning a sentinel that a
 * caller might use as an index: the two cards must be distinct, and every
 * legal pair of distinct cards has a type.
 */
static inline int hand_type_of(const struct hand_type *types, int card1, int card2)
{
    int rank1 = RANK_OF(card1), rank2 = RANK_OF(card2);
    int high = rank1 > rank2 ? rank1 : rank2;
    int low  = rank1 > rank2 ? rank2 : rank1;
    bool suited = (rank1 != rank2) && (SUIT_OF(card1) == SUIT_OF(card2));

    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        if (types[i].high_rank == high && types[i].low_rank == low &&
            types[i].suited == suited)
            return i;
    }

    fprintf(stderr, "FAIL  cards %d and %d are not a legal starting hand\n",
            card1, card2);
    exit(1);
}

/* Writes the 50 cards that remain once a hand's two cards are removed.
 * `deck` must have room for 50 entries; returns how many were written. */
static inline int deck_without(const int *hole_cards, int *deck)
{
    int count = 0;

    for (int card = 0; card < NUM_CARDS; card++) {
        if (card != hole_cards[0] && card != hole_cards[1])
            deck[count++] = card;
    }
    return count;
}

#endif /* HANDS169_H */
