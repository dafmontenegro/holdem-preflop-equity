/* potential.c — the exact potential of all 169 starting hands.
 *
 * WHAT IT COMPUTES
 * ----------------
 * For each of the 169 starting hand types, how often the player's finished
 * five-card hand lands in each of the nine categories, once all five board
 * cards are out.
 *
 * This is a hand's *potential*, or versatility: how often it makes a flush, a
 * straight, trips and so on. It is **not** the probability of winning. A hand
 * that makes straights often can still lose far more often than one that does
 * not, because what wins a pot is holding the better hand, not the rarer
 * category. 32o makes more straights than AKo and loses badly to it. Winning
 * is equity, and equity is what equity_mc.c estimates.
 *
 * HOW IT IS COMPUTED: ENUMERATION, NOT SIMULATION
 * -----------------------------------------------
 * Every number here is exact. For a given hand, 50 cards remain, so there are
 * C(50,5) = 2,118,760 possible boards, and the program visits every one of
 * them and classifies the resulting seven cards. No sampling and no random
 * numbers are involved, so there is no error bar to report: the counts are
 * the counts.
 *
 * Boards are enumerated by five nested loops over deck positions with
 * a < b < c < d < e. Forcing the positions to ascend is what makes each set
 * of five cards appear exactly once instead of 5! = 120 times, which is the
 * difference between counting boards and counting deal orders. The two give
 * the same probabilities, since the 120 cancels in the ratio, but only one of
 * them is 120 times less work.
 *
 * Total work is 169 x 2,118,760 = about 358 million classifications.
 *
 * SELF-VALIDATION
 * ---------------
 * The program checks its own output before printing it, and refuses to print
 * anything if the check fails. See `validate_against_known_counts`.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cards.h"
#include "eval7.h"
#include "hands169.h"

#define BOARDS_PER_HAND 2118760L   /* C(50,5) */

/* The result for one starting hand type. */
struct potential {
    long long category_counts[NUM_CATEGORIES];
    long long improves_board;   /* boards where the player's cards raise the
                                 * category the board makes on its own */
};

/* The number of seven-card hands of each category, over all C(52,7) =
 * 133,784,560 seven-card hands. Published figures, used here as the outside
 * reference that the enumeration has to agree with.
 *
 * Kept in the same order as `enum hand_category`. */
static const long long SEVEN_CARD_COUNTS[NUM_CATEGORIES] = {
    23294460, 58627800, 31433400, 6461620, 6180020,
     4047644,  3473184,   224848,   41584
};

/* Enumerates all 2,118,760 boards for one starting hand and counts where the
 * player's finished hand lands.
 *
 * The board is also classified on its own, as five cards. When the player's
 * seven-card category is higher than the board's five-card category, the
 * board counts as improved.
 *
 * A LIMITATION, STATED PLAINLY
 * ----------------------------
 * "Improves the board" only notices a jump to a *higher category*. It does
 * not notice an improvement inside one category, such as holding a higher
 * flush than the board makes, or a bigger two pair. So it understates how
 * often the player's cards matter. It is reported because it is useful and
 * cheap, not because it is the right measure; a proper one is on the list for
 * the next phase.
 */
static void enumerate_hand(const struct hand_type *type, struct potential *result)
{
    int deck[50];
    int deck_size = deck_without(type->cards, deck);

    int cards[7];
    cards[0] = type->cards[0];
    cards[1] = type->cards[1];

    memset(result, 0, sizeof *result);

    for (int a = 0;     a < deck_size; a++)
    for (int b = a + 1; b < deck_size; b++)
    for (int c = b + 1; c < deck_size; c++)
    for (int d = c + 1; d < deck_size; d++)
    for (int e = d + 1; e < deck_size; e++) {
        cards[2] = deck[a];
        cards[3] = deck[b];
        cards[4] = deck[c];
        cards[5] = deck[d];
        cards[6] = deck[e];

        int player_category = score_category(hand_score(cards, 7));
        int board_category  = score_category(hand_score(cards + 2, 5));

        result->category_counts[player_category]++;
        if (player_category > board_category)
            result->improves_board++;
    }
}

/* Checks the whole table against the published seven-card hand counts.
 *
 * THE ARGUMENT
 * ------------
 * Take any set of seven cards. It can be split into "two private cards plus a
 * five-card board" in C(7,2) = 21 ways, and each of those 21 splits is one of
 * the boards this program enumerated for one of the 1,326 concrete starting
 * hands. The player's category is the same in all 21, because it depends on
 * the seven cards and not on which two are private.
 *
 * So summing the counts over all 1,326 starting hands counts every
 * seven-card hand exactly 21 times. Dividing by 21 must give the published
 * number of seven-card hands of that category, for all nine categories.
 *
 * The sum over 1,326 hands is obtained from the 169 types by weighting each
 * with its combo count, which is what `combos` is for.
 *
 * WHY THIS IS A STRONG CHECK
 * --------------------------
 * It is not a spot check of a few hands: it ties the entire table, all nine
 * categories at once, to a count computed independently and long before this
 * code. A bug in straight detection, in flush detection, or in the loop
 * bounds would have to be compensated by an exactly offsetting bug elsewhere
 * to pass. It also catches a double-counted or skipped board, since the
 * division by 21 would leave a remainder.
 *
 * It does not check the ordering of hands within a category: these are
 * category counts, and kickers never enter. That is checked exhaustively
 * elsewhere, by the distinct-value count in test_eval7.c.
 *
 * Returns true when everything matches, and reports every discrepancy.
 */
static bool validate_against_known_counts(const struct hand_type *types,
                                          const struct potential *results)
{
    bool all_matched = true;

    for (int category = 0; category < NUM_CATEGORIES; category++) {
        long long weighted_sum = 0;

        for (int i = 0; i < NUM_HAND_TYPES; i++)
            weighted_sum += (long long)types[i].combos
                          * results[i].category_counts[category];

        long long quotient  = weighted_sum / 21;
        long long remainder = weighted_sum % 21;

        if (remainder != 0) {
            fprintf(stderr,
                    "  FAIL  %-15s weighted sum %lld is not divisible by 21 "
                    "(remainder %lld): a board was counted twice or missed\n",
                    CATEGORY_NAMES[category], weighted_sum, remainder);
            all_matched = false;
        } else if (quotient != SEVEN_CARD_COUNTS[category]) {
            fprintf(stderr,
                    "  FAIL  %-15s counted %lld seven-card hands, published "
                    "figure is %lld\n",
                    CATEGORY_NAMES[category], quotient, SEVEN_CARD_COUNTS[category]);
            all_matched = false;
        } else {
            fprintf(stderr, "  ok    %-15s %12lld seven-card hands\n",
                    CATEGORY_NAMES[category], quotient);
        }
    }

    /* Each hand's categories must also account for every board exactly once. */
    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        long long sum = 0;
        for (int category = 0; category < NUM_CATEGORIES; category++)
            sum += results[i].category_counts[category];

        if (sum != BOARDS_PER_HAND) {
            fprintf(stderr,
                    "  FAIL  %s: categories sum to %lld, expected %ld boards\n",
                    types[i].label, sum, BOARDS_PER_HAND);
            all_matched = false;
        }
    }

    return all_matched;
}

static void write_csv(FILE *out, const struct hand_type *types,
                      const struct potential *results)
{
    fprintf(out, "hand,family,combos,boards,"
                 "high_card,pair,two_pair,three_of_a_kind,straight,flush,"
                 "full_house,four_of_a_kind,straight_flush,improves_board\n");

    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        fprintf(out, "%s,%s,%d,%ld",
                types[i].label, types[i].family, types[i].combos, BOARDS_PER_HAND);

        for (int category = 0; category < NUM_CATEGORIES; category++)
            fprintf(out, ",%lld", results[i].category_counts[category]);

        fprintf(out, ",%lld\n", results[i].improves_board);
    }
}

int main(int argc, char **argv)
{
    const char *output_path = (argc > 1) ? argv[1] : NULL;

    static struct hand_type types[NUM_HAND_TYPES];
    static struct potential results[NUM_HAND_TYPES];

    int type_count = hand_types_all(types);
    if (type_count != NUM_HAND_TYPES) {
        fprintf(stderr, "FAIL  enumerated %d hand types, expected %d\n",
                type_count, NUM_HAND_TYPES);
        return 1;
    }

    /* The combo counts must add up to the 1,326 concrete starting hands, or
     * every weighted average in the project is wrong. */
    int combo_total = 0;
    for (int i = 0; i < NUM_HAND_TYPES; i++)
        combo_total += types[i].combos;

    if (combo_total != 1326) {
        fprintf(stderr, "FAIL  combos sum to %d, expected 1326\n", combo_total);
        return 1;
    }

    fprintf(stderr, "Exact potential of the 169 starting hands\n");
    fprintf(stderr, "  %d hand types x %ld boards = %lld classifications\n\n",
            NUM_HAND_TYPES, BOARDS_PER_HAND,
            (long long)NUM_HAND_TYPES * BOARDS_PER_HAND);

    clock_t started = clock();
    for (int i = 0; i < NUM_HAND_TYPES; i++)
        enumerate_hand(&types[i], &results[i]);
    double seconds = (double)(clock() - started) / CLOCKS_PER_SEC;

    fprintf(stderr, "Enumerated in %.1f s. Validating against the published "
                    "seven-card hand counts:\n", seconds);

    if (!validate_against_known_counts(types, results)) {
        fprintf(stderr, "\nValidation failed. No output written: these counts "
                        "are wrong and must not be published.\n");
        return 1;
    }

    fprintf(stderr, "\nAll nine categories match, and every hand accounts for "
                    "all %ld boards.\n", BOARDS_PER_HAND);

    if (output_path == NULL) {
        write_csv(stdout, types, results);
    } else {
        FILE *out = fopen(output_path, "w");
        if (out == NULL) {
            fprintf(stderr, "FAIL  cannot write %s\n", output_path);
            return 1;
        }
        write_csv(out, types, results);
        fclose(out);
        fprintf(stderr, "Wrote %s\n", output_path);
    }
    return 0;
}
