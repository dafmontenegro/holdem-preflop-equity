/* cards.h — how a card is represented, parsed and printed.
 *
 * ENCODING
 * --------
 * A card is a single integer 0..51:
 *
 *     card = rank * 4 + suit
 *
 *     rank  0..12  for 2,3,4,5,6,7,8,9,T,J,Q,K,A   (0 is the deuce, 12 the ace)
 *     suit  0..3   for c,d,h,s                      (clubs, diamonds, hearts, spades)
 *
 * So the nine of clubs is rank 7, suit 0, card 7*4+0 = 28. Going back:
 * rank = card / 4 and suit = card % 4.
 *
 * Two properties of this encoding are worth stating, because the rest of the
 * engine leans on both:
 *
 *   1. Rank order is numeric order. A higher rank is a higher number, with no
 *      lookup table, which is what lets the evaluator pack a hand's strength
 *      into one comparable integer (see eval7.h).
 *
 *   2. Suits have no order of their own. In Texas Hold'em no suit beats
 *      another, so suit indices are labels, not values. Nothing in the engine
 *      may compare two suits for strength; suits are only ever compared for
 *      equality ("same suit?") or permuted (see the suit isomorphism used to
 *      collapse equivalent matchups).
 *
 * This is the same encoding used by the exploration phase under exploration/,
 * so its C programs and this engine can be compared card for card.
 *
 * RANKS vs. VALUES: a note on vocabulary
 * --------------------------------------
 * Throughout the engine, "rank" means the 0..12 index of a card's face value
 * (what a player calls a deuce or an ace), and "category" means the kind of
 * five-card hand (pair, flush, full house...). Poker texts sometimes use
 * "hand rank" for the second meaning; this code never does.
 */
#ifndef CARDS_H
#define CARDS_H

#include <stdbool.h>
#include <stddef.h>   /* NULL, used when parsing text into cards */

#define NUM_CARDS  52
#define NUM_RANKS  13
#define NUM_SUITS   4

/* Rank indices, named so the tests and the table builders read like poker
 * rather than like arithmetic. */
enum {
    RANK_2 = 0, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8,
    RANK_9, RANK_T, RANK_J, RANK_Q, RANK_K, RANK_A
};

/* Suit indices. Order is arbitrary and carries no strength; it exists only so
 * that a suit can be named in a test or printed back to the reader. */
enum { SUIT_CLUBS = 0, SUIT_DIAMONDS, SUIT_HEARTS, SUIT_SPADES };

#define CARD_OF(rank, suit)  ((rank) * NUM_SUITS + (suit))
#define RANK_OF(card)        ((card) / NUM_SUITS)
#define SUIT_OF(card)        ((card) % NUM_SUITS)

/* Characters used to read and write cards. Index by rank or suit. */
static const char RANK_CHARS[NUM_RANKS + 1] = "23456789TJQKA";
static const char SUIT_CHARS[NUM_SUITS + 1] = "cdhs";

/* Parses a two-character card such as "9c", "Td" or "As".
 *
 * Returns the card 0..51, or -1 if the text is not a card. Rank characters are
 * accepted in either case ("tD" works); "10" is not accepted, because every
 * card must be exactly two characters for a hand like "AsKs" to be read
 * without ambiguity.
 */
static inline int card_parse(const char *text)
{
    if (text == NULL || text[0] == '\0' || text[1] == '\0')
        return -1;

    int rank = -1, suit = -1;

    for (int r = 0; r < NUM_RANKS; r++) {
        char c = text[0];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');          /* fold case: "t" becomes "T" */
        if (RANK_CHARS[r] == c) {
            rank = r;
            break;
        }
    }
    for (int s = 0; s < NUM_SUITS; s++) {
        char c = text[1];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');          /* fold case: "S" becomes "s" */
        if (SUIT_CHARS[s] == c) {
            suit = s;
            break;
        }
    }
    if (rank < 0 || suit < 0)
        return -1;

    return CARD_OF(rank, suit);
}

/* Parses a run of concatenated cards such as "AsKs" or "9c8c7d6h2s".
 *
 * Writes the cards into `out` and returns how many were read, or -1 if the
 * text is malformed or holds more than `capacity` cards. Whitespace between
 * cards is allowed, so both "AsKs" and "As Ks" are read the same way.
 */
static inline int cards_parse(const char *text, int *out, int capacity)
{
    int count = 0;

    while (*text != '\0') {
        if (*text == ' ' || *text == '\t') {
            text++;
            continue;
        }
        if (count >= capacity)
            return -1;

        int card = card_parse(text);
        if (card < 0)
            return -1;

        out[count++] = card;
        text += 2;
    }
    return count;
}

/* Writes a card as two characters plus a terminator, so `out` needs 3 bytes.
 * Returns `out` so the result can be used directly in a printf argument. */
static inline char *card_format(int card, char *out)
{
    out[0] = RANK_CHARS[RANK_OF(card)];
    out[1] = SUIT_CHARS[SUIT_OF(card)];
    out[2] = '\0';
    return out;
}

/* True when every card in the list is a legal card and no card repeats.
 *
 * Every entry point that takes cards from outside the engine checks this.
 * A duplicate is the one input error that silently produces a plausible but
 * meaningless answer: the evaluator would happily read "AsAs" as a pair of
 * aces, so a deck that deals the same card twice would inflate quads and
 * full houses instead of crashing.
 */
static inline bool cards_are_distinct(const int *cards, int count)
{
    bool seen[NUM_CARDS] = { false };

    for (int i = 0; i < count; i++) {
        int card = cards[i];
        if (card < 0 || card >= NUM_CARDS || seen[card])
            return false;
        seen[card] = true;
    }
    return true;
}

#endif /* CARDS_H */
