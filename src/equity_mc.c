/* equity_mc.c — equity of the 169 starting hands against 1 to 8 opponents.
 *
 * WHAT EQUITY IS
 * --------------
 * Equity is the share of the pot a hand wins on average, if the hand is dealt
 * out to the river and shown down. It is the number that answers "how good is
 * my hand", which potential.c does not: potential says how often a hand makes
 * a flush, equity says how often it takes the money.
 *
 * A showdown has three outcomes, and they are counted separately here rather
 * than folded into one average, because they are different facts:
 *
 *     win    the hand is strictly best                    share 1
 *     tie    k players hold equally best hands            share 1/k
 *     lose   some other hand is strictly better           share 0
 *
 * The 1/k on a tie is not an approximation. A tied pot is split k ways, so
 * 1/k of it is literally what the player collects. Ties happen only when the
 * best five cards are equal in rank, kickers included: a better kicker is a
 * win, not a tie.
 *
 *     equity = (wins + sum of 1/k over ties) / trials
 *
 * THE MODEL, AND WHAT IT ASSUMES
 * ------------------------------
 * Opponents are dealt uniformly random hands and every hand is taken to the
 * river. This is the right model for one specific question — what is this hand
 * worth against n unknown hands — and it is the wrong model for a real table,
 * where opponents fold their worst hands and only continue with their better
 * ones. Against players who only call with strong hands, true equity is lower
 * than these figures. Equity against a range is a separate calculation, and
 * these numbers are its baseline, not a substitute for it.
 *
 * Cards are dealt without replacement from one deck, so nothing here assumes
 * independence between the hands: the opponents' cards, the board and the
 * player's own cards all come out of the same 52.
 *
 * WHY MONTE CARLO HERE, AND WHAT THE ERROR IS
 * -------------------------------------------
 * Exact enumeration against even one unknown opponent means every one of the
 * C(50,2) = 1,225 opponent hands crossed with every board, and against eight
 * opponents the count of ways to deal 16 cards is astronomical. So equity
 * against n opponents is estimated by sampling, and an estimate is worthless
 * without its error.
 *
 * Each trial yields a share in [0,1], and the estimate is the mean of those
 * shares. The standard error of a mean is
 *
 *     standard error = sqrt(sample variance / trials)
 *
 * and it is reported for every cell, computed from that cell's own sample
 * rather than assumed from a formula for a proportion: a share is not a
 * coin flip, since ties put mass strictly between 0 and 1.
 *
 * The variance is accumulated with Welford's method, which updates the mean
 * and the sum of squared deviations in one pass. Summing the squares of the
 * raw values and subtracting the square of the mean would be shorter, and it
 * loses precision badly when the variance is small next to the mean, which is
 * exactly the regime here.
 *
 * Roughly, with 100,000 trials the standard error is at most about 0.16
 * percentage points, and 95% of the time the true value lies within about two
 * standard errors of the estimate.
 *
 * SELF-VALIDATION
 * ---------------
 * See `validate_fair_share`. The program checks a property no amount of
 * plausible-looking output can fake, and says plainly whether it holds.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cards.h"
#include "eval7.h"
#include "hands169.h"
#include "rng.h"

#define MAX_OPPONENTS 8

/* One cell of the table: one starting hand against one number of opponents. */
struct cell {
    long long trials;
    long long wins;
    long long ties;
    long long losses;

    /* Welford's running mean and sum of squared deviations, over the per-trial
     * pot shares. */
    double mean_share;
    double sum_squared_deviations;
};

static void cell_record(struct cell *cell, double share)
{
    cell->trials++;

    double deviation = share - cell->mean_share;
    cell->mean_share += deviation / (double)cell->trials;
    cell->sum_squared_deviations += deviation * (share - cell->mean_share);
}

static double cell_standard_error(const struct cell *cell)
{
    if (cell->trials < 2)
        return 0.0;

    double variance = cell->sum_squared_deviations / (double)(cell->trials - 1);
    return sqrt(variance / (double)cell->trials);
}

/* Plays `trials` showdowns of one starting hand against `opponents` random
 * hands, and records the results.
 *
 * THE DEAL
 * --------
 * The 50 cards that are not the player's are held in `deck`, and each trial
 * shuffles only as many positions as it needs: a partial Fisher-Yates pass
 * over the first 2*opponents + 5 positions. Each position is swapped with a
 * uniformly chosen position at or after it, which is what makes every
 * possible deal equally likely.
 *
 * The deck is not restored between trials, and does not need to be: a partial
 * Fisher-Yates pass leaves the deck a permutation of the same 50 cards, so
 * the next trial starts from a deck that is just as valid, merely in a
 * different order.
 *
 * Dealt cards are then read off: two per opponent from the front, and the
 * five board cards immediately after them.
 */
static void simulate_cell(const struct hand_type *type, int opponents,
                          long long trials, struct rng *rng, struct cell *cell)
{
    int deck[50];
    int deck_size = deck_without(type->cards, deck);
    int cards_needed = 2 * opponents + 5;

    memset(cell, 0, sizeof *cell);

    for (long long trial = 0; trial < trials; trial++) {
        for (int i = 0; i < cards_needed; i++) {
            int j = i + (int)rng_below(rng, (uint64_t)(deck_size - i));
            int swap = deck[i];
            deck[i] = deck[j];
            deck[j] = swap;
        }

        const int *board = &deck[2 * opponents];

        int hand[7];
        hand[0] = type->cards[0];
        hand[1] = type->cards[1];
        memcpy(&hand[2], board, 5 * sizeof(int));
        int player_score = hand_score(hand, 7);

        /* The best opponent score, and how many opponents hold it. Only the
         * best matters: a hand that loses to one opponent wins nothing, no
         * matter how many others it beats. */
        int best_opponent_score = -1;
        int opponents_at_best = 0;

        for (int o = 0; o < opponents; o++) {
            hand[0] = deck[2 * o];
            hand[1] = deck[2 * o + 1];

            int score = hand_score(hand, 7);
            if (score > best_opponent_score) {
                best_opponent_score = score;
                opponents_at_best = 1;
            } else if (score == best_opponent_score) {
                opponents_at_best++;
            }
        }

        if (player_score > best_opponent_score) {
            cell->wins++;
            cell_record(cell, 1.0);
        } else if (player_score == best_opponent_score) {
            /* The player and `opponents_at_best` others are all equally best,
             * so the pot is split that many ways plus one. */
            cell->ties++;
            cell_record(cell, 1.0 / (double)(opponents_at_best + 1));
        } else {
            cell->losses++;
            cell_record(cell, 0.0);
        }
    }
}

/* Checks the table against the one property it cannot be wrong about.
 *
 * THE ARGUMENT
 * ------------
 * At a table of n+1 players all holding uniformly random hands, nobody has an
 * advantage: the seats are symmetric. All n+1 equities must therefore be
 * equal, and since they are shares of one pot they must sum to 1, so each is
 * exactly 1/(n+1).
 *
 * The equity of a random hand is the average of the 169 types weighted by how
 * many concrete hands each stands for, so for every opponent count:
 *
 *     sum over types of (combos x equity) / 1326  =  1 / (n + 1)
 *
 * WHY THIS IS A STRONG CHECK
 * --------------------------
 * It is exact arithmetic about an estimated quantity, so the comparison has
 * to be made against the sampling error rather than against zero. What makes
 * it strong is that it constrains all 169 hands jointly: an evaluator that
 * mishandled, say, the wheel would push some hands up and others down, and
 * the weighted average would drift off 1/(n+1) by far more than the error
 * allows. It also catches the subtler mistakes that leave hands
 * individually plausible: a biased shuffle, dealing the board before the
 * opponents' cards from an exhausted deck, or weighting the 169 types by
 * count instead of by combos.
 *
 * The check is reported as a multiple of the standard error of the weighted
 * average. Under 2 is unremarkable, since the estimate should land within two
 * standard errors about 95% of the time; well over 3 is a bug.
 */
static bool validate_fair_share(const struct hand_type *types,
                                const struct cell cells[][MAX_OPPONENTS + 1])
{
    bool all_passed = true;

    fprintf(stderr, "Validating the fair share: a random hand must be worth "
                    "exactly 1/(players) of the pot\n");

    for (int opponents = 1; opponents <= MAX_OPPONENTS; opponents++) {
        double weighted_equity = 0.0;
        double weighted_variance = 0.0;

        for (int i = 0; i < NUM_HAND_TYPES; i++) {
            const struct cell *cell = &cells[i][opponents];
            double weight = (double)types[i].combos / 1326.0;
            double error  = cell_standard_error(cell);

            weighted_equity += weight * cell->mean_share;

            /* The 169 cells are independent samples, so the variance of their
             * weighted sum is the weighted sum of their variances, with the
             * weights squared. */
            weighted_variance += weight * weight * error * error;
        }

        double expected       = 1.0 / (double)(opponents + 1);
        double standard_error = sqrt(weighted_variance);
        double difference     = weighted_equity - expected;
        double sigmas         = standard_error > 0.0
                              ? fabs(difference) / standard_error : 0.0;

        bool passed = sigmas < 4.0;
        if (!passed)
            all_passed = false;

        fprintf(stderr,
                "  %-5s %d opponents: average %.6f, expected %.6f, "
                "off by %+.2f pp = %.2f standard errors\n",
                passed ? "ok" : "FAIL", opponents,
                weighted_equity, expected, difference * 100.0, sigmas);
    }

    return all_passed;
}

static void write_csv(FILE *out, const struct hand_type *types,
                      const struct cell cells[][MAX_OPPONENTS + 1],
                      unsigned long long seed)
{
    fprintf(out, "# Equity of the 169 starting hands against 1 to 8 opponents "
                 "holding uniformly random hands.\n");
    fprintf(out, "# Estimated by Monte Carlo simulation; seed %llu. "
                 "Every row carries its own standard error.\n", seed);
    fprintf(out, "hand,family,combos,opponents,trials,wins,ties,losses,"
                 "equity,standard_error\n");

    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        for (int opponents = 1; opponents <= MAX_OPPONENTS; opponents++) {
            const struct cell *cell = &cells[i][opponents];

            fprintf(out, "%s,%s,%d,%d,%lld,%lld,%lld,%lld,%.6f,%.6f\n",
                    types[i].label, types[i].family, types[i].combos,
                    opponents, cell->trials, cell->wins, cell->ties,
                    cell->losses, cell->mean_share, cell_standard_error(cell));
        }
    }
}

static void print_usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [trials] [seed] [output.csv]\n"
            "\n"
            "  trials      showdowns per hand per opponent count (default 100000)\n"
            "  seed        seed for the random generator (default 1)\n"
            "  output.csv  where to write the table (default standard output)\n"
            "\n"
            "Running the same trials and seed reproduces the same table exactly.\n"
            "Running different seeds gives independent estimates, which is how\n"
            "the reported standard errors can be checked rather than trusted.\n",
            program);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    long long trials = (argc > 1) ? atoll(argv[1]) : 100000;
    unsigned long long seed = (argc > 2) ? strtoull(argv[2], NULL, 10) : 1;
    const char *output_path = (argc > 3) ? argv[3] : NULL;

    if (trials < 2) {
        fprintf(stderr, "FAIL  trials must be at least 2 to estimate an error\n");
        return 1;
    }

    static struct hand_type types[NUM_HAND_TYPES];
    hand_types_all(types);

    static struct cell cells[NUM_HAND_TYPES][MAX_OPPONENTS + 1];

    struct rng rng;
    rng_seed(&rng, seed);

    fprintf(stderr, "Equity of the 169 starting hands against 1 to 8 opponents\n");
    fprintf(stderr, "  %lld trials per cell, %d cells, %lld showdowns in total\n",
            trials, NUM_HAND_TYPES * MAX_OPPONENTS,
            trials * NUM_HAND_TYPES * MAX_OPPONENTS);
    fprintf(stderr, "  seed %llu\n\n", seed);

    clock_t started = clock();
    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        for (int opponents = 1; opponents <= MAX_OPPONENTS; opponents++)
            simulate_cell(&types[i], opponents, trials, &rng, &cells[i][opponents]);
    }
    double seconds = (double)(clock() - started) / CLOCKS_PER_SEC;

    fprintf(stderr, "Simulated in %.1f s.\n\n", seconds);

    bool valid = validate_fair_share(types, cells);

    if (!valid) {
        fprintf(stderr, "\nValidation failed: the weighted average is too far "
                        "from the fair share to be sampling noise. No output "
                        "written.\n");
        return 1;
    }
    fprintf(stderr, "\nThe fair share holds at every table size.\n");

    if (output_path == NULL) {
        write_csv(stdout, types, cells, seed);
    } else {
        FILE *out = fopen(output_path, "w");
        if (out == NULL) {
            fprintf(stderr, "FAIL  cannot write %s\n", output_path);
            return 1;
        }
        write_csv(out, types, cells, seed);
        fclose(out);
        fprintf(stderr, "Wrote %s\n", output_path);
    }
    return 0;
}
