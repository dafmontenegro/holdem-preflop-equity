/* suits.h — relabelling suits, and why that makes the exact matrix possible.
 *
 * THE IDEA
 * --------
 * No suit beats another in Hold'em. So take any deal and rename the suits —
 * every club becomes a heart, every heart a club — and you get another deal
 * that is exactly as likely, in which every player holds a hand of the same
 * strength against the same opposition. Renaming suits is a symmetry of the
 * game.
 *
 * There are 4! = 24 ways to rename four suits. Any two situations that a
 * renaming turns into each other have identical probabilities, so only one of
 * them needs to be computed. This is what shrinks the head-to-head matrix
 * from a calculation measured in days to one measured in minutes.
 *
 * It is also the reason there are 169 starting hands instead of 1,326: under
 * renaming, what survives of a two-card hand is its two ranks and whether the
 * suits match.
 *
 * HOW IT IS USED
 * --------------
 * Computing the equity of hand type A against hand type B means averaging
 * over all the concrete ways both can be dealt. Two reductions cut that down,
 * and they are applied in this order:
 *
 * 1. FIX A REPRESENTATIVE FOR THE FIRST HAND. The average over all concrete
 *    pairs equals the average taken with the first hand fixed at any one of
 *    its forms. The argument: pick a concrete hand `a` of type A. Renaming
 *    suits maps `a` to the chosen representative a0, and maps the opponent
 *    hands available against `a` one-to-one onto those available against a0,
 *    preserving every equity. So the inner average is the same whichever `a`
 *    is chosen, and it is enough to compute it once, at a0.
 *
 *    This needs each concrete hand of type A to face the same number of
 *    opponent hands of type B, which is checked rather than assumed: see the
 *    invariants reported by the matrix program.
 *
 * 2. GROUP THE OPPONENT HANDS THAT ARE STILL EQUIVALENT. With a0 fixed, only
 *    the renamings that leave a0 alone are still available. Those form a0's
 *    *stabiliser*, and opponent hands in the same orbit under it have
 *    identical equity against a0, so one of each orbit is computed and
 *    weighted by the size of its orbit.
 *
 *    The stabiliser's size depends on the shape of a0, which is why the
 *    saving is uneven:
 *
 *      suited (AcKc)  all 3! = 6 renamings of the other three suits
 *      a pair (AcAd)  the two suits used may swap, and the two unused ones
 *                     may swap: 2 x 2 = 4
 *      offsuit (AcKd) the two suits used are told apart by which rank they
 *                     carry, so only the two unused may swap: 2
 *
 * Together these take the 207,025 hand-type-against-opponent-hand matchups
 * down to 93,769. Halving that again by computing only one of each pair of
 * mirrored cells leaves 47,086.
 */
#ifndef SUITS_H
#define SUITS_H

#include "cards.h"

#define NUM_SUIT_PERMUTATIONS 24

/* The 24 renamings of the four suits. Entry [i][s] is the suit that suit `s`
 * becomes under renaming `i`. */
static const int SUIT_PERMUTATIONS[NUM_SUIT_PERMUTATIONS][NUM_SUITS] = {
    {0,1,2,3}, {0,1,3,2}, {0,2,1,3}, {0,2,3,1}, {0,3,1,2}, {0,3,2,1},
    {1,0,2,3}, {1,0,3,2}, {1,2,0,3}, {1,2,3,0}, {1,3,0,2}, {1,3,2,0},
    {2,0,1,3}, {2,0,3,1}, {2,1,0,3}, {2,1,3,0}, {2,3,0,1}, {2,3,1,0},
    {3,0,1,2}, {3,0,2,1}, {3,1,0,2}, {3,1,2,0}, {3,2,0,1}, {3,2,1,0}
};

/* Applies a renaming to one card. The rank is untouched. */
static inline int suit_permute_card(const int *permutation, int card)
{
    return CARD_OF(RANK_OF(card), permutation[SUIT_OF(card)]);
}

/* A two-card hand, reduced to a single number that does not depend on the
 * order the two cards are written in.
 *
 * Used to compare hands for identity and to pick one canonical member of an
 * orbit, so the only thing that matters is that it is one-to-one on unordered
 * pairs. */
static inline int hand_key(int card1, int card2)
{
    int low  = card1 < card2 ? card1 : card2;
    int high = card1 < card2 ? card2 : card1;

    return low * NUM_CARDS + high;
}

/* A hand's stabiliser: the renamings that leave the hand unchanged.
 *
 * Writes their indices into `out`, which needs room for 24, and returns how
 * many there are. The count is always 6 for a suited hand, 4 for a pair and 2
 * for an offsuit hand.
 */
static inline int suit_stabiliser(int card1, int card2, int *out)
{
    int key = hand_key(card1, card2);
    int count = 0;

    for (int i = 0; i < NUM_SUIT_PERMUTATIONS; i++) {
        const int *permutation = SUIT_PERMUTATIONS[i];
        int moved = hand_key(suit_permute_card(permutation, card1),
                             suit_permute_card(permutation, card2));
        if (moved == key)
            out[count++] = i;
    }
    return count;
}

/* The canonical form of a hand within its orbit under a set of renamings.
 *
 * Returns the smallest key the hand takes under any of them. Two hands land
 * on the same canonical key exactly when some renaming in the set turns one
 * into the other, which is what makes the key usable as an orbit identifier.
 */
static inline int hand_canonical_key(int card1, int card2,
                                     const int *permutation_indices, int count)
{
    int smallest = -1;

    for (int i = 0; i < count; i++) {
        const int *permutation = SUIT_PERMUTATIONS[permutation_indices[i]];
        int key = hand_key(suit_permute_card(permutation, card1),
                           suit_permute_card(permutation, card2));
        if (smallest < 0 || key < smallest)
            smallest = key;
    }
    return smallest;
}

#endif /* SUITS_H */
