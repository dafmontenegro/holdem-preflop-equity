/* refined_potential.c — the potential of each hand, counting only the hands
 * its own two cards actually contribute to.
 *
 * THE PROBLEM WITH PLAIN POTENTIAL
 * --------------------------------
 * potential.c answers "how often does my finished five-card hand land in each
 * category". That is the right question, and it has a flattering answer. 72o
 * reaches two pair or better on 34% of boards, which sounds like a playable
 * hand and is not: most of those are two pair sitting on the board, which
 * every other player at the table has as well. A category you share with
 * everyone is worth nothing.
 *
 * So this program asks the sharper question twice over.
 *
 * WHAT IT COMPUTES
 * ----------------
 * 1. THE CATEGORY REACHED USING AT LEAST ONE OF YOUR OWN CARDS. Not the best
 *    five of your seven, but the best five that includes at least one card
 *    only you hold. Comparing this against plain potential shows how much of
 *    a hand's apparent potential was really the board's.
 *
 * 2. WHETHER YOUR CARDS IMPROVED ON THE BOARD AT ALL, measured properly. The
 *    exploration phase approximated this by asking whether your cards raised
 *    the *category* the board makes, and said so: it misses a king-high flush
 *    where the board alone makes a nine-high flush, because both are "a
 *    flush". Here the comparison is between full hand scores, which rank
 *    within a category as well as across them, so a better flush counts as
 *    the improvement it is. Both measures are reported side by side, so the
 *    size of that gap is visible rather than asserted.
 *
 * THE OBSERVATION THAT MAKES THIS CHEAP
 * -------------------------------------
 * Your seven cards have C(7,5) = 21 five-card subsets, and **exactly one of
 * them is the board on its own** — the one that leaves out both of your
 * cards. Every other subset uses at least one. So:
 *
 *     best five using one of your cards  =  best over the other 20 subsets
 *     best five overall                  =  max(that, the board's own score)
 *
 * which is why the 20 subsets are enumerated explicitly: 5 of them pair your
 * first card with four board cards, 5 pair your second, and 10 use both of
 * yours with three from the board.
 *
 * And they are not always enumerated. If the best five of your seven already
 * beats the board on its own, then the best five must be using one of your
 * cards, so the restricted answer equals the unrestricted one and the 20
 * subsets tell us nothing new. Only when the two scores are equal — when your
 * cards add nothing to the top of your hand — do the 20 need checking, to
 * find what the best hand using one of them would have been. That is a
 * minority of boards for most hands, which is what keeps this at a few times
 * the cost of plain potential rather than ten times.
 *
 * Everything here is exact: all 2,118,760 boards per hand, no sampling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cards.h"
#include "eval7.h"
#include "hands169.h"

#define BOARDS_PER_HAND 2118760L   /* C(50,5) */

struct refined {
    /* The category of the best five cards, however they are made. This must
     * reproduce potential.c exactly, which is the cross-check that the two
     * programs agree about the same boards. */
    long long best_category[NUM_CATEGORIES];

    /* The category of the best five that uses at least one of the player's
     * own cards. */
    long long hole_category[NUM_CATEGORIES];

    /* How the player's cards improve on the hand the board makes on its own,
     * split into the three kinds of improvement there are. Every board falls
     * into exactly one of these four buckets, and they sum to the total.
     *
     * The split matters because "my cards improved my hand" covers two very
     * different events, and lumping them together is misleading in both
     * directions. A board making a pair of sevens is improved by holding a
     * king, which becomes the top kicker, and it is improved by holding a
     * pair of kings, which becomes the hand. Only the second changes what the
     * player has; the first changes who wins a pot between two players who
     * both have the board's pair.
     *
     * The three are told apart straight from the score layout (see eval7.h):
     * the category occupies the top field and the hand's defining rank the
     * next one down, so comparing those two fields against the board's own
     * score says which kind of improvement this is. */
    long long raises_category;        /* a better category than the board's   */
    long long raises_defining_rank;   /* same category, bigger pair, higher
                                       * flush, higher straight               */
    long long improves_kicker_only;   /* same category and same defining rank,
                                       * a better side card                   */
    long long no_improvement;         /* the board's hand is the player's hand */
};

static const long long SEVEN_CARD_COUNTS[NUM_CATEGORIES] = {
    23294460, 58627800, 31433400, 6461620, 6180020,
     4047644,  3473184,   224848,   41584
};

/* The best five-card score that uses at least one of the two hole cards.
 *
 * The 20 qualifying subsets, enumerated in the three shapes they come in:
 * one hole card with four from the board, the other hole card with four from
 * the board, and both hole cards with three from the board.
 */
static int best_score_using_a_hole_card(const int *hole, const int *board)
{
    int best = -1;
    int five[5];

    /* One hole card plus four of the five board cards: leave out one board
     * card at a time. */
    for (int which = 0; which < 2; which++) {
        for (int left_out = 0; left_out < 5; left_out++) {
            int count = 0;
            five[count++] = hole[which];
            for (int i = 0; i < 5; i++) {
                if (i != left_out)
                    five[count++] = board[i];
            }

            int score = hand_score(five, 5);
            if (score > best)
                best = score;
        }
    }

    /* Both hole cards plus three of the five board cards: leave out two. */
    for (int first_out = 0; first_out < 5; first_out++) {
        for (int second_out = first_out + 1; second_out < 5; second_out++) {
            int count = 0;
            five[count++] = hole[0];
            five[count++] = hole[1];
            for (int i = 0; i < 5; i++) {
                if (i != first_out && i != second_out)
                    five[count++] = board[i];
            }

            int score = hand_score(five, 5);
            if (score > best)
                best = score;
        }
    }

    return best;
}

static void enumerate_hand(const struct hand_type *type, struct refined *result)
{
    int deck[50];
    int deck_size = deck_without(type->cards, deck);

    int seven[7];
    seven[0] = type->cards[0];
    seven[1] = type->cards[1];
    int *board = &seven[2];

    memset(result, 0, sizeof *result);

    for (int a = 0;     a < deck_size; a++)
    for (int b = a + 1; b < deck_size; b++)
    for (int c = b + 1; c < deck_size; c++)
    for (int d = c + 1; d < deck_size; d++)
    for (int e = d + 1; e < deck_size; e++) {
        board[0] = deck[a];
        board[1] = deck[b];
        board[2] = deck[c];
        board[3] = deck[d];
        board[4] = deck[e];

        int best_score  = hand_score(seven, 7);
        int board_score = hand_score(board, 5);

        /* When the best five beats the board, it must be using one of the
         * player's cards, so the restricted answer is the same score and the
         * 20 subsets need not be visited. */
        int hole_score = (best_score > board_score)
                       ? best_score
                       : best_score_using_a_hole_card(type->cards, board);

        result->best_category[score_category(best_score)]++;
        result->hole_category[score_category(hole_score)]++;

        if (score_category(hole_score) > score_category(board_score))
            result->raises_category++;
        else if (score_category(hole_score) < score_category(board_score))
            result->no_improvement++;
        else if (score_defining_rank(hole_score) > score_defining_rank(board_score))
            result->raises_defining_rank++;
        else if (hole_score > board_score)
            result->improves_kicker_only++;
        else
            result->no_improvement++;
    }
}

static bool validate(const struct hand_type *types, const struct refined *results)
{
    bool all_passed = true;

    /* 1. The unrestricted counts must reproduce the published seven-card hand
     *    counts, by the same 21-way argument potential.c uses: every set of
     *    seven cards splits into two private plus a board in C(7,2) = 21 ways.
     *    This ties this program to the same outside reference, and to
     *    potential.c, without either having to trust the other. */
    for (int category = 0; category < NUM_CATEGORIES; category++) {
        long long weighted = 0;
        for (int i = 0; i < NUM_HAND_TYPES; i++)
            weighted += (long long)types[i].combos * results[i].best_category[category];

        if (weighted % 21 != 0 || weighted / 21 != SEVEN_CARD_COUNTS[category]) {
            fprintf(stderr, "  FAIL  %-15s counted %lld seven-card hands, "
                    "published figure is %lld\n", CATEGORY_NAMES[category],
                    weighted / 21, SEVEN_CARD_COUNTS[category]);
            all_passed = false;
        }
    }
    if (all_passed)
        fprintf(stderr, "  ok    the unrestricted counts match the published "
                        "seven-card hand counts\n");

    /* 2. Both distributions must account for every board. */
    int bad = 0;
    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        long long best = 0, hole = 0;
        for (int category = 0; category < NUM_CATEGORIES; category++) {
            best += results[i].best_category[category];
            hole += results[i].hole_category[category];
        }
        if (best != BOARDS_PER_HAND || hole != BOARDS_PER_HAND)
            bad++;
    }
    if (bad > 0) {
        fprintf(stderr, "  FAIL  %d hands do not account for all %ld boards\n",
                bad, BOARDS_PER_HAND);
        all_passed = false;
    } else {
        fprintf(stderr, "  ok    both distributions account for all %ld boards, "
                        "for every hand\n", BOARDS_PER_HAND);
    }

    /* 3. The four kinds of improvement are exclusive and exhaustive, so they
     *    must account for every board. A board counted twice, or not at all,
     *    shows up here. */
    bad = 0;
    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        long long sum = results[i].raises_category
                      + results[i].raises_defining_rank
                      + results[i].improves_kicker_only
                      + results[i].no_improvement;
        if (sum != BOARDS_PER_HAND)
            bad++;
    }
    if (bad > 0) {
        fprintf(stderr, "  FAIL  %d hands do not sort every board into exactly "
                        "one kind of improvement\n", bad);
        all_passed = false;
    } else {
        fprintf(stderr, "  ok    every board sorts into exactly one of the four "
                        "kinds of improvement\n");
    }

    /* 4. Requiring a hole card can only ever make the hand worse, never
     *    better, so the restricted distribution must be no stronger than the
     *    unrestricted one. Checked as a stochastic ordering: for every
     *    category, "this category or better" must be no more likely when a
     *    hole card is required. */
    bad = 0;
    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        for (int category = 0; category < NUM_CATEGORIES; category++) {
            long long best = 0, hole = 0;
            for (int c = category; c < NUM_CATEGORIES; c++) {
                best += results[i].best_category[c];
                hole += results[i].hole_category[c];
            }
            if (hole > best)
                bad++;
        }
    }
    if (bad > 0) {
        fprintf(stderr, "  FAIL  %d cases where requiring a hole card made the "
                        "hand stronger\n", bad);
        all_passed = false;
    } else {
        fprintf(stderr, "  ok    requiring a hole card never makes the hand "
                        "stronger, in any category of any hand\n");
    }

    return all_passed;
}

static void write_csv(FILE *out, const struct hand_type *types,
                      const struct refined *results)
{
    fprintf(out, "# Potential of the 169 starting hands, counted twice: the best "
                 "five cards however\n");
    fprintf(out, "# they are made, and the best five that uses at least one of "
                 "your own two cards.\n");
    fprintf(out, "# Exact: complete enumeration of all %ld boards per hand.\n",
            BOARDS_PER_HAND);

    fprintf(out, "hand,family,combos,boards");
    for (int category = 0; category < NUM_CATEGORIES; category++)
        fprintf(out, ",best_%s", CATEGORY_NAMES[category]);
    for (int category = 0; category < NUM_CATEGORIES; category++)
        fprintf(out, ",hole_%s", CATEGORY_NAMES[category]);
    fprintf(out, ",raises_category,raises_defining_rank,improves_kicker_only,"
                 "no_improvement\n");

    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        fprintf(out, "%s,%s,%d,%ld",
                types[i].label, types[i].family, types[i].combos, BOARDS_PER_HAND);

        for (int category = 0; category < NUM_CATEGORIES; category++)
            fprintf(out, ",%lld", results[i].best_category[category]);
        for (int category = 0; category < NUM_CATEGORIES; category++)
            fprintf(out, ",%lld", results[i].hole_category[category]);

        fprintf(out, ",%lld,%lld,%lld,%lld\n",
                results[i].raises_category, results[i].raises_defining_rank,
                results[i].improves_kicker_only, results[i].no_improvement);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s output.csv\n", argv[0]);
        return 1;
    }

    static struct hand_type types[NUM_HAND_TYPES];
    static struct refined results[NUM_HAND_TYPES];

    hand_types_all(types);

    fprintf(stderr, "Refined potential: the category reached using at least one "
                    "of your own cards\n");
    fprintf(stderr, "  %d hand types x %ld boards\n\n",
            NUM_HAND_TYPES, BOARDS_PER_HAND);

    clock_t started = clock();
    for (int i = 0; i < NUM_HAND_TYPES; i++)
        enumerate_hand(&types[i], &results[i]);
    double seconds = (double)(clock() - started) / CLOCKS_PER_SEC;

    fprintf(stderr, "Enumerated in %.1f s. Validating:\n", seconds);

    if (!validate(types, results)) {
        fprintf(stderr, "\nValidation failed. No output written.\n");
        return 1;
    }

    FILE *out = fopen(argv[1], "w");
    if (out == NULL) {
        fprintf(stderr, "FAIL  cannot write %s\n", argv[1]);
        return 1;
    }
    write_csv(out, types, results);
    fclose(out);
    fprintf(stderr, "\nWrote %s\n", argv[1]);

    return 0;
}
