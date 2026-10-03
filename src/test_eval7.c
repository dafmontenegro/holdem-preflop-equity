/* test_eval7.c — the evaluator's test suite.
 *
 * The suite has two halves, and they check different things.
 *
 * NAMED HANDS answer "does the evaluator read this specific hand the way a
 * dealer would?". They are written out card by card, with the expected reading
 * in the description, so a reader can check the test itself against the rules
 * of poker without running anything. They cover the cases where an evaluator
 * is most likely to be wrong: the wheel, kickers in every category, two trips,
 * three pairs, and hands where the player's own cards do not play.
 *
 * EXHAUSTIVE INVARIANTS answer "is the evaluator right about every hand in the
 * deck?". Named hands can only ever check the cases someone thought of; these
 * check all of them, by enumerating entire hand spaces and comparing the
 * result against counts that were published long before this code existed:
 *
 *   1. The nine category counts over all 2,598,960 five-card hands.
 *   2. The number of distinct hand values among those: exactly 7,462. This is
 *      the one that validates the *ordering*, kickers included. The category
 *      counts would still pass if, say, two-pair kickers were ignored; the
 *      distinct-value count would not, because merging two hands that should
 *      rank differently lowers the total.
 *   3. The nine category counts over all 133,784,560 seven-card hands, which
 *      is the hand space the engine actually works in.
 *
 * Together these two halves are what lets the project claim the evaluator is
 * correct rather than merely tested. Reference figures for 1 and 3 are the
 * standard enumerations of poker hands; 7,462 is the long-known number of
 * distinct five-card hand values.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "eval7.h"

static int checks_run    = 0;
static int checks_failed = 0;

/* Records one assertion. Failures are printed as they happen and counted, so
 * that a broken evaluator reports everything that is wrong in one run instead
 * of stopping at the first problem. */
static void check(int passed, const char *description)
{
    checks_run++;
    if (!passed) {
        checks_failed++;
        printf("  FAIL  %s\n", description);
    }
}

/* Scores a hand written as concatenated cards, e.g. "AsKsQsJsTs".
 *
 * Refuses malformed input and duplicate cards by aborting the test run: a
 * typo in a test would otherwise quietly score a hand nobody intended, and a
 * test that passes for the wrong reason is worse than no test. */
static int score_of(const char *text)
{
    int cards[7];
    int count = cards_parse(text, cards, 7);

    if (count < 5 || count > 7 || !cards_are_distinct(cards, count)) {
        printf("  ABORT  not a legal hand of 5 to 7 distinct cards: \"%s\"\n", text);
        exit(2);
    }
    return hand_score(cards, count);
}

/* Checks that a hand is read as the expected category. */
static void check_category(const char *hand, int expected, const char *description)
{
    int actual = score_category(score_of(hand));

    check(actual == expected, description);
    if (actual != expected)
        printf("         \"%s\" read as %s, expected %s\n",
               hand, CATEGORY_NAMES[actual], CATEGORY_NAMES[expected]);
}

/* Checks that the first hand beats the second. */
static void check_beats(const char *winner, const char *loser, const char *description)
{
    check(score_of(winner) > score_of(loser), description);
}

/* Checks that two hands split the pot. */
static void check_ties(const char *one, const char *other, const char *description)
{
    check(score_of(one) == score_of(other), description);
}

/* ------------------------------------------------------------------------
 * Part 1: named hands
 * ------------------------------------------------------------------------ */

static void test_categories_are_recognised(void)
{
    printf("Categories, one five-card hand each\n");

    check_category("AsKsQsJsTs", CAT_STRAIGHT_FLUSH, "royal flush is a straight flush");
    check_category("9h8h7h6h5h", CAT_STRAIGHT_FLUSH, "nine-high straight flush");
    check_category("7s7h7d7c2s", CAT_FOUR_OF_KIND,   "four sevens");
    check_category("7s7h7dQcQs", CAT_FULL_HOUSE,     "sevens full of queens");
    check_category("Kc Tc 7c 4c 2c", CAT_FLUSH,      "king-high flush");
    check_category("9s8h7d6c5s", CAT_STRAIGHT,       "nine-high straight");
    check_category("8s8h8dAc4s", CAT_THREE_OF_KIND,  "three eights");
    check_category("9s9h5d5cKs", CAT_TWO_PAIR,       "nines and fives");
    check_category("9s9hKd7c2s", CAT_PAIR,           "one pair of nines");
    check_category("AsJd8c5h3s", CAT_HIGH_CARD,      "ace high, nothing made");
}

static void test_category_order(void)
{
    printf("Category order, strongest beats next strongest\n");

    check_beats("9h8h7h6h5h", "7s7h7d7c2s", "straight flush beats four of a kind");
    check_beats("7s7h7d7c2s", "7s7h7dQcQs", "four of a kind beats a full house");
    check_beats("7s7h7dQcQs", "KcTc7c4c2c", "full house beats a flush");
    check_beats("KcTc7c4c2c", "9s8h7d6c5s", "flush beats a straight");
    check_beats("9s8h7d6c5s", "8s8h8dAc4s", "straight beats three of a kind");
    check_beats("8s8h8dAc4s", "9s9h5d5cKs", "three of a kind beats two pair");
    check_beats("9s9h5d5cKs", "9s9hKd7c2s", "two pair beats one pair");
    check_beats("9s9hKd7c2s", "AsJd8c5h3s", "one pair beats ace high");
}

static void test_the_wheel(void)
{
    printf("The ace-low straight (the wheel)\n");

    check_category("5s4h3d2cAs", CAT_STRAIGHT,
                   "A-2-3-4-5 is a straight, with the ace playing low");
    check_category("5c4c3c2cAc", CAT_STRAIGHT_FLUSH,
                   "A-2-3-4-5 in one suit is a straight flush");

    check_beats("6s5h4d3c2s", "5s4h3d2cAs",
                "6-high straight beats the wheel: the ace is low, not high");
    check_beats("As Ks Qs Js Ts", "5c4c3c2cAc",
                "royal flush beats the steel wheel");
    check_beats("5s4h3d2cAs", "AsKdQcJs9h",
                "the wheel is still a straight, and beats ace-king high");

    /* A-K-Q-J-T is a straight too, and it is the one the ace tops. Checking
     * both ends of the ace's double role is the point of this pair. */
    check_category("AsKdQcJhTs", CAT_STRAIGHT, "A-K-Q-J-T is an ace-high straight");
    check_beats("AsKdQcJhTs", "KsQdJcTh9s", "ace-high straight beats king-high");

    /* The gap case: an ace with a four and no five is not a straight, which is
     * the mistake a naive "ace wraps around" rule makes. */
    check_category("AsKdQc4h3s", CAT_HIGH_CARD, "A-K-Q-4-3 is not a straight");
    check_category("4s3h2dAcKs", CAT_HIGH_CARD, "K-A-2-3-4 does not wrap around");
}

static void test_kickers_in_every_category(void)
{
    printf("Kickers, category by category\n");

    /* Four of a kind: the fifth card decides. */
    check_beats("7s7h7d7cAs", "7s7h7d7cKs", "quad sevens, ace kicker beats king kicker");

    /* Four of a kind where the kicker is itself a pair. Only one card of it
     * plays, so a king kicker beats a queen kicker even though the queens are
     * the pair. */
    check_beats("7s7h7d7cKsKd", "7s7h7d7cQsQd",
                "quads with a king kicker beat quads with a queen kicker");

    /* Full house: trips first, then the pair. */
    check_beats("8s8h8d2c2s", "7s7h7dAcAs", "eights full beats sevens full, pair irrelevant");
    check_beats("7s7h7dAcAs", "7s7h7dKcKs", "sevens full of aces beats sevens full of kings");

    /* Flush: compared all five cards down. */
    check_beats("AcTc7c4c2c", "KcTc7c4c2c", "ace-high flush beats king-high flush");
    check_beats("AcTc7c4c3c", "AcTc7c4c2c", "flushes tie down to the last card, three beats deuce");

    /* Three of a kind: two kickers, in order. */
    check_beats("8s8h8dAc4s", "8s8h8dKc4s", "trip eights, ace kicker beats king kicker");
    check_beats("8s8h8dAc5s", "8s8h8dAc4s", "trip eights, ace-five beats ace-four");

    /* Two pair: high pair, low pair, then the kicker. */
    check_beats("9s9h5d5cKs", "8s8h5d5cKs", "nines and fives beats eights and fives");
    check_beats("9s9h6d6cKs", "9s9h5d5cKs", "nines and sixes beats nines and fives");
    check_beats("9s9h5d5cKs", "9s9h5d5cQs", "nines and fives, king kicker beats queen kicker");

    /* One pair: three kickers, in order. */
    check_beats("9s9hAd7c2s", "9s9hKd7c2s", "pair of nines, ace kicker beats king kicker");
    check_beats("9s9hAd8c2s", "9s9hAd7c2s", "pair of nines, ace-eight beats ace-seven");
    check_beats("9s9hAd7c3s", "9s9hAd7c2s", "pair of nines, third kicker decides");

    /* High card: all five ranks. */
    check_beats("AsJd8c5h4s", "AsJd8c5h3s", "ace high, fifth card decides");
    check_beats("AsQd8c5h3s", "AsJd8c5h3s", "ace high, queen beats jack as second card");
}

static void test_the_subtle_seven_card_cases(void)
{
    printf("Seven-card readings that trip up an evaluator\n");

    /* Two trips cannot both play. The lower trips becomes the pair, so this is
     * kings full of fives, not "two trips" and not fives full of kings. */
    check_category("KsKhKd5c5h5s2d", CAT_FULL_HOUSE, "two trips make a full house");
    check_ties("KsKhKd5c5h5s2d", "KsKhKd5c5h2d3c",
               "K-K-K-5-5-5-2 reads the same as kings full of fives");
    check_beats("KsKhKd5c5h5s2d", "QsQhQdJcJhJs2d",
                "kings full of fives beats queens full of jacks");

    /* There is no "two trips plus a separate pair" case to test: that needs
     * 3+3+2 = 8 cards, and a Hold'em player sees seven. So whenever two trips
     * appear, the lower trips is the only thing available to play as the pair.
     * The evaluator still takes the higher of the two candidates, which costs
     * nothing and keeps it correct if it is ever handed more cards. */

    /* Three pairs: only two play, and the kicker is the highest card outside
     * them, which here is the third pair's five, not the deuce. */
    check_category("9s9h7d7c5h5s2d", CAT_TWO_PAIR, "three pairs make two pair");
    check_ties("9s9h7d7c5h5s2d", "9s9h7d7c5h3c2d",
               "nines and sevens with a five kicker, from three pairs");
    check_beats("9s9h7d7c5h5s2d", "9s9h7d7c4h4s2d",
                "the third pair still matters as a kicker: five beats four");

    /* Quads plus trips is possible with exactly seven cards. The trips rank
     * serves as the kicker. */
    check_category("7s7h7d7c5h5s5d", CAT_FOUR_OF_KIND, "quads and trips is four of a kind");
    check_ties("7s7h7d7c5h5s5d", "7s7h7d7c5h2s3d",
               "quad sevens with a five kicker, however the five arrived");

    /* The best five of seven may use one of the player's cards, or neither.
     * Here the straight is entirely on the board. */
    check_ties("9s8h7d6c5s2h3d", "9s8h7d6c5sAhKd",
               "a straight on the board reads the same whatever the player holds");

    /* And here the player's cards improve nothing: both players play the
     * board, so the hand is a split. */
    check_ties("AsKsQsJsTs2h3d", "AsKsQsJsTs4c9h",
               "a royal flush on the board is a split pot");

    /* A flush of six or seven cards still plays only its best five. */
    check_ties("AcKcQcJc9c2c", "AcKcQcJc9c3c",
               "six cards of one suit play the best five: the sixth is ignored");
    check_beats("AcKcQcJc9c2c", "AcKcQcJc8c2c",
                "within those five, the fifth card still decides");

    /* A straight and a flush in the same seven cards: the flush plays.
     * The five clubs are 9-8-7-3-2, a nine-high flush; the seven cards also
     * contain the straight 5-6-7-8-9. The flush is the higher category, so it
     * is the hand. Note the clubs themselves are not consecutive, which is
     * what keeps this from being a straight flush. */
    check_category("9c8c7c3c2c6h5h", CAT_FLUSH,
                   "nine-high flush outranks the straight in the same hand");

    /* Four to a straight flush plus a fifth of the suit elsewhere: the flush
     * plays, and it must not be read as a straight flush. */
    check_category("9c8c7c6cAc5h4h", CAT_FLUSH,
                   "five clubs that are not consecutive are a flush, not a straight flush");
}

static void test_card_parsing(void)
{
    printf("Card encoding and parsing\n");

    check(card_parse("2c") == 0,  "2c is card 0, the first of the deck");
    check(card_parse("As") == 51, "As is card 51, the last of the deck");
    check(card_parse("9c") == 28, "9c is card 28, as the encoding note says");
    check(card_parse("tD") == card_parse("Td"), "rank and suit letters are case-insensitive");
    check(card_parse("1c") < 0,   "1c is not a card");
    check(card_parse("10c") == card_parse("1c"), "\"10\" is not accepted as a rank");
    check(card_parse("Ax") < 0,   "x is not a suit");
    check(card_parse("A")  < 0,   "a rank without a suit is not a card");

    /* Every card survives a round trip through text. */
    int round_trips_intact = 1;
    for (int card = 0; card < NUM_CARDS; card++) {
        char text[3];
        card_format(card, text);
        if (card_parse(text) != card)
            round_trips_intact = 0;
    }
    check(round_trips_intact, "all 52 cards survive formatting and parsing back");

    int cards[7];
    check(cards_parse("AsKs", cards, 7) == 2,        "two cards read from \"AsKs\"");
    check(cards_parse("As Ks", cards, 7) == 2,       "spaces between cards are allowed");
    check(cards_parse("AsKsQsJsTs2h3d", cards, 7) == 7, "seven cards read");
    check(cards_parse("AsKsQsJsTs2h3d4c", cards, 7) == -1, "more cards than capacity is refused");
    check(cards_parse("AsK", cards, 7) == -1,        "a trailing half-card is refused");

    cards_parse("AsAs", cards, 7);
    check(!cards_are_distinct(cards, 2), "a repeated card is detected");
    cards_parse("AsKs", cards, 7);
    check(cards_are_distinct(cards, 2), "two different cards are accepted");
}

/* ------------------------------------------------------------------------
 * Part 2: exhaustive invariants
 * ------------------------------------------------------------------------ */

/* The nine category counts over all C(52,5) = 2,598,960 five-card hands.
 * Straights and flushes here exclude straight flushes, which are counted on
 * their own, matching how the evaluator assigns one category per hand. */
static const long long FIVE_CARD_COUNTS[NUM_CATEGORIES] = {
    1302540,  /* high card       */
    1098240,  /* pair            */
     123552,  /* two pair        */
      54912,  /* three of a kind */
      10200,  /* straight        */
       5108,  /* flush           */
       3744,  /* full house      */
        624,  /* four of a kind  */
         40   /* straight flush  */
};

/* The same nine counts over all C(52,7) = 133,784,560 seven-card hands, each
 * hand counted once under the category of its best five cards. */
static const long long SEVEN_CARD_COUNTS[NUM_CATEGORIES] = {
    23294460, /* high card       */
    58627800, /* pair            */
    31433400, /* two pair        */
     6461620, /* three of a kind */
     6180020, /* straight        */
     4047644, /* flush           */
     3473184, /* full house      */
      224848, /* four of a kind  */
       41584  /* straight flush  */
};

/* The number of distinct five-card hand values in poker. Two hands share a
 * value exactly when they split the pot. */
#define DISTINCT_FIVE_CARD_VALUES 7462

static void compare_counts(const long long *actual, const long long *expected,
                           long long total_hands, const char *label)
{
    long long sum = 0;

    for (int category = 0; category < NUM_CATEGORIES; category++) {
        char description[128];
        snprintf(description, sizeof description, "%s: %s count is %lld",
                 label, CATEGORY_NAMES[category], expected[category]);
        check(actual[category] == expected[category], description);

        if (actual[category] != expected[category])
            printf("         counted %lld\n", actual[category]);

        sum += actual[category];
    }

    char description[128];
    snprintf(description, sizeof description, "%s: the nine categories sum to %lld",
             label, total_hands);
    check(sum == total_hands, description);
}

static void test_all_five_card_hands(void)
{
    printf("Every five-card hand in the deck (2,598,960)\n");

    long long counts[NUM_CATEGORIES] = { 0 };

    /* Distinct scores are tracked in a flat lookup table rather than a set.
     * A score is at most (8 << 20) plus five rank slots, so the table is a
     * few million bytes: cheap, and it makes the count exact. */
    const int score_table_size = (NUM_CATEGORIES << 20);
    char *seen = calloc((size_t)score_table_size, 1);
    if (seen == NULL) {
        printf("  ABORT  out of memory for the distinct-score table\n");
        exit(2);
    }
    long long distinct = 0;

    int cards[5];
    for (int a = 0;     a < NUM_CARDS; a++)
    for (int b = a + 1; b < NUM_CARDS; b++)
    for (int c = b + 1; c < NUM_CARDS; c++)
    for (int d = c + 1; d < NUM_CARDS; d++)
    for (int e = d + 1; e < NUM_CARDS; e++) {
        cards[0] = a; cards[1] = b; cards[2] = c; cards[3] = d; cards[4] = e;

        int score = hand_score(cards, 5);
        counts[score_category(score)]++;

        if (!seen[score]) {
            seen[score] = 1;
            distinct++;
        }
    }
    free(seen);

    compare_counts(counts, FIVE_CARD_COUNTS, 2598960, "five-card hands");

    char description[128];
    snprintf(description, sizeof description,
             "five-card hands: exactly %d distinct hand values (kickers included)",
             DISTINCT_FIVE_CARD_VALUES);
    check(distinct == DISTINCT_FIVE_CARD_VALUES, description);
    if (distinct != DISTINCT_FIVE_CARD_VALUES)
        printf("         counted %lld distinct values\n", distinct);
}

static void test_all_seven_card_hands(void)
{
    printf("Every seven-card hand in the deck (133,784,560)\n");

    long long counts[NUM_CATEGORIES] = { 0 };

    /* The same loop also checks the two ways of reaching a score against each
     * other. The exact calculations summarise a board once and add each
     * player's two cards to a copy, instead of reading all seven cards; that
     * shortcut carries most of the engine's running time, so it is checked on
     * every hand in the deck rather than on a sample. The first five cards
     * stand in for the board and the last two for the player's own. */
    long long summary_path_disagreements = 0;

    int cards[7];
    for (int a = 0;     a < NUM_CARDS; a++)
    for (int b = a + 1; b < NUM_CARDS; b++)
    for (int c = b + 1; c < NUM_CARDS; c++)
    for (int d = c + 1; d < NUM_CARDS; d++)
    for (int e = d + 1; e < NUM_CARDS; e++)
    for (int f = e + 1; f < NUM_CARDS; f++)
    for (int g = f + 1; g < NUM_CARDS; g++) {
        cards[0] = a; cards[1] = b; cards[2] = c; cards[3] = d;
        cards[4] = e; cards[5] = f; cards[6] = g;

        int score = hand_score(cards, 7);
        counts[score_category(score)]++;

        struct card_summary board;
        card_summary_init(&board);
        card_summary_add_cards(&board, cards, 5);

        struct card_summary with_hole = board;
        card_summary_add(&with_hole, cards[5]);
        card_summary_add(&with_hole, cards[6]);

        if (score_summary(&with_hole) != score)
            summary_path_disagreements++;
    }

    compare_counts(counts, SEVEN_CARD_COUNTS, 133784560, "seven-card hands");

    check(summary_path_disagreements == 0,
          "seven-card hands: adding cards to a board summary gives the same score "
          "as reading all seven");
    if (summary_path_disagreements != 0)
        printf("         %lld hands disagreed\n", summary_path_disagreements);
}

int main(void)
{
    printf("Evaluator test suite\n");
    printf("====================\n\n");

    test_card_parsing();
    test_categories_are_recognised();
    test_category_order();
    test_the_wheel();
    test_kickers_in_every_category();
    test_the_subtle_seven_card_cases();
    test_all_five_card_hands();
    test_all_seven_card_hands();

    printf("\n%d checks, %d failed\n", checks_run, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}
