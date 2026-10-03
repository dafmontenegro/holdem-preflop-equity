/* headsup.c — the exact 169 x 169 head-to-head matrix.
 *
 * WHAT IT COMPUTES
 * ----------------
 * For every ordered pair of starting hand types, how often the first hand
 * wins, ties and loses against the second when both are dealt to the river.
 * All 28,561 cells, by complete enumeration of every board. No sampling, no
 * error bars.
 *
 * This is the table everything else in the project is built on. The exact
 * equity of a hand against a random hand is a row of it; the ranking of the
 * 169 hands is that column sorted; equity against an opponent playing the top
 * X% of hands is a weighted sum of a row. Computing it once, exactly, means
 * none of those inherits a sampling error.
 *
 * HOW THE WORK IS MADE FEASIBLE
 * -----------------------------
 * Taken literally, the definition is out of reach: averaging over all 812,175
 * pairs of concrete starting hands, each with C(48,5) = 1,712,304 boards, is
 * about 1.4 trillion hand evaluations.
 *
 * Three reductions bring it down to 161 billion, which runs in minutes:
 *
 * 1. The first hand is fixed at one representative per type, because renaming
 *    suits makes every concrete form of a type equivalent. 812,175 pairs
 *    become 207,025 matchups.
 *
 * 2. Opponent hands that the remaining renamings map onto each other are
 *    computed once and weighted by how many they stand for. 207,025 become
 *    93,769. Both reductions are explained in suits.h.
 *
 * 3. Only one of each pair of mirrored cells is computed: A against B gives B
 *    against A by swapping wins and losses. 93,769 become 47,086.
 *
 * A fourth saving is not a reduction in cases but in cost per case: the board
 * is summarised as it is built, one card per loop level, so the innermost loop
 * adds a single card rather than re-reading five, and both players' hands are
 * scored from the same board summary. See eval7.h.
 *
 * COUNTS, NOT FRACTIONS
 * ---------------------
 * Every number is kept as an exact integer count of boards, scaled so that a
 * cell counts boards over *all* concrete deals of that pair of types, not
 * just over the representative's. That scaling is what makes the mirrored
 * cells line up exactly, and it lets every invariant be checked in integer
 * arithmetic with no tolerance:
 *
 *     wins + ties + losses == deals x 1,712,304     for every cell
 *     equity(A,B) + equity(B,A) == 1                for every pair
 *     equity(A,A) == 1/2                            on the diagonal
 *     the average over all 1,326 hands == 1/2       for the whole matrix
 *
 * Equity is the rational number (2 x wins + ties) / (2 x total), since a
 * head-to-head tie is always split two ways. The division happens only when
 * the table is written out.
 */

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "cards.h"
#include "eval7.h"
#include "hands169.h"
#include "suits.h"

#define BOARDS_PER_MATCHUP 1712304L   /* C(48,5) */
#define CONCRETE_HANDS     1326       /* C(52,2) */
#define OPPONENT_HANDS     1225       /* C(50,2) */

/* One cell of the matrix. Counts are over all concrete deals of the pair, so
 * `wins + ties + losses` is `deals * BOARDS_PER_MATCHUP`. */
struct cell {
    long long wins;
    long long ties;
    long long losses;
};

/* One unit of work: a hero type against one representative opponent hand. */
struct matchup {
    int hero_type;
    int opponent_type;
    int opponent_cards[2];
    int stands_for;      /* opponent hands this representative speaks for */
};

/* Everything the worker threads share. */
struct job {
    const struct hand_type *types;
    const struct matchup *matchups;
    int matchup_count;

    atomic_int next_matchup;
    atomic_int completed;
};

/* Counts wins, ties and losses for one matchup, over every board.
 *
 * The board is summarised incrementally: each loop level holds the summary of
 * the board cards chosen so far, so the innermost level adds one card to a
 * four-card summary instead of reading five cards from scratch. Both players
 * are then scored by adding their two cards to copies of that summary.
 */
static void enumerate_matchup(const struct hand_type *hero,
                              const int *opponent_cards,
                              struct cell *out)
{
    /* The 48 cards that are neither player's. */
    int deck[48];
    int deck_size = 0;
    for (int card = 0; card < NUM_CARDS; card++) {
        if (card != hero->cards[0] && card != hero->cards[1] &&
            card != opponent_cards[0] && card != opponent_cards[1])
            deck[deck_size++] = card;
    }

    long long wins = 0, ties = 0, losses = 0;

    struct card_summary empty;
    card_summary_init(&empty);

    for (int a = 0; a < deck_size; a++) {
        struct card_summary after_a = empty;
        card_summary_add(&after_a, deck[a]);

        for (int b = a + 1; b < deck_size; b++) {
            struct card_summary after_b = after_a;
            card_summary_add(&after_b, deck[b]);

            for (int c = b + 1; c < deck_size; c++) {
                struct card_summary after_c = after_b;
                card_summary_add(&after_c, deck[c]);

                for (int d = c + 1; d < deck_size; d++) {
                    struct card_summary after_d = after_c;
                    card_summary_add(&after_d, deck[d]);

                    for (int e = d + 1; e < deck_size; e++) {
                        struct card_summary board = after_d;
                        card_summary_add(&board, deck[e]);

                        struct card_summary hero_hand = board;
                        card_summary_add(&hero_hand, hero->cards[0]);
                        card_summary_add(&hero_hand, hero->cards[1]);

                        struct card_summary opponent_hand = board;
                        card_summary_add(&opponent_hand, opponent_cards[0]);
                        card_summary_add(&opponent_hand, opponent_cards[1]);

                        int hero_score     = score_summary(&hero_hand);
                        int opponent_score = score_summary(&opponent_hand);

                        if (hero_score > opponent_score)      wins++;
                        else if (hero_score == opponent_score) ties++;
                        else                                   losses++;
                    }
                }
            }
        }
    }

    out->wins   = wins;
    out->ties   = ties;
    out->losses = losses;
}

/* A worker thread's private accumulator, merged into the matrix at the end so
 * that no two threads ever write the same memory. */
struct worker {
    struct job *job;
    struct cell *matrix;    /* NUM_HAND_TYPES x NUM_HAND_TYPES */
};

static void *worker_run(void *argument)
{
    struct worker *worker = argument;
    struct job *job = worker->job;

    for (;;) {
        int index = atomic_fetch_add(&job->next_matchup, 1);
        if (index >= job->matchup_count)
            break;

        const struct matchup *matchup = &job->matchups[index];
        const struct hand_type *hero = &job->types[matchup->hero_type];

        struct cell counts;
        enumerate_matchup(hero, matchup->opponent_cards, &counts);

        /* Scale to all concrete deals: the representative opponent hand
         * speaks for `stands_for` opponent hands, and the hero's fixed
         * representative speaks for all `combos` of its type. */
        long long weight = (long long)matchup->stands_for * hero->combos;

        struct cell *cell = &worker->matrix[matchup->hero_type * NUM_HAND_TYPES
                                          + matchup->opponent_type];
        cell->wins   += weight * counts.wins;
        cell->ties   += weight * counts.ties;
        cell->losses += weight * counts.losses;

        int done = atomic_fetch_add(&job->completed, 1) + 1;
        if (done % 2000 == 0 || done == job->matchup_count)
            fprintf(stderr, "  %d of %d matchups\n", done, job->matchup_count);
    }
    return NULL;
}

/* Builds the list of matchups to compute, and the count of concrete deals for
 * every cell.
 *
 * `deals[A][B]` is how many ordered pairs of concrete starting hands have
 * types A and B and share no card. It is counted for the whole matrix, not
 * just the half that gets enumerated, because the mirrored cells need it too.
 */
static int build_matchups(const struct hand_type *types,
                          struct matchup *matchups,
                          long long *deals)
{
    int count = 0;

    for (int hero_type = 0; hero_type < NUM_HAND_TYPES; hero_type++) {
        const struct hand_type *hero = &types[hero_type];

        int stabiliser[NUM_SUIT_PERMUTATIONS];
        int stabiliser_size = suit_stabiliser(hero->cards[0], hero->cards[1],
                                              stabiliser);

        /* First pass: for each opponent hand, which orbit it belongs to. */
        int orbit_key[OPPONENT_HANDS];
        int orbit_type[OPPONENT_HANDS];
        int orbit_cards[OPPONENT_HANDS][2];
        int opponent_count = 0;

        for (int first = 0; first < NUM_CARDS; first++) {
            if (first == hero->cards[0] || first == hero->cards[1])
                continue;

            for (int second = first + 1; second < NUM_CARDS; second++) {
                if (second == hero->cards[0] || second == hero->cards[1])
                    continue;

                int type = hand_type_of(types, first, second);
                deals[hero_type * NUM_HAND_TYPES + type] += hero->combos;

                orbit_key[opponent_count]      = hand_canonical_key(first, second,
                                                      stabiliser, stabiliser_size);
                orbit_type[opponent_count]     = type;
                orbit_cards[opponent_count][0] = first;
                orbit_cards[opponent_count][1] = second;
                opponent_count++;
            }
        }

        if (opponent_count != OPPONENT_HANDS) {
            fprintf(stderr, "FAIL  %s faces %d opponent hands, expected %d\n",
                    hero->label, opponent_count, OPPONENT_HANDS);
            exit(1);
        }

        /* Second pass: one matchup per orbit, weighted by the orbit's size.
         * Only the cells at or above the diagonal are enumerated; the rest
         * are mirrored afterwards. */
        for (int i = 0; i < opponent_count; i++) {
            bool already_seen = false;
            for (int j = 0; j < i; j++) {
                if (orbit_key[j] == orbit_key[i]) {
                    already_seen = true;
                    break;
                }
            }
            if (already_seen || orbit_type[i] < hero_type)
                continue;

            int stands_for = 0;
            for (int j = 0; j < opponent_count; j++) {
                if (orbit_key[j] == orbit_key[i])
                    stands_for++;
            }

            matchups[count].hero_type        = hero_type;
            matchups[count].opponent_type    = orbit_type[i];
            matchups[count].opponent_cards[0] = orbit_cards[i][0];
            matchups[count].opponent_cards[1] = orbit_cards[i][1];
            matchups[count].stands_for       = stands_for;
            count++;
        }
    }
    return count;
}

/* Fills the cells below the diagonal from their mirror above it.
 *
 * A wins exactly when B loses, and they tie together, so the mirrored cell is
 * the same counts with wins and losses swapped. This is exact rather than
 * approximate because the counts are scaled to all concrete deals, and a pair
 * of types has the same number of deals read in either direction.
 */
static void mirror_lower_triangle(struct cell *matrix)
{
    for (int a = 0; a < NUM_HAND_TYPES; a++) {
        for (int b = a + 1; b < NUM_HAND_TYPES; b++) {
            const struct cell *above = &matrix[a * NUM_HAND_TYPES + b];
            struct cell *below = &matrix[b * NUM_HAND_TYPES + a];

            below->wins   = above->losses;
            below->ties   = above->ties;
            below->losses = above->wins;
        }
    }
}

/* Checks the finished matrix. Every check is exact integer arithmetic: there
 * is nothing estimated here, so there is no tolerance to choose and no room
 * for "close enough".
 *
 * Returns true when everything holds, and reports every failure.
 */
static bool validate(const struct hand_type *types, const struct cell *matrix,
                     const long long *deals)
{
    bool all_passed = true;
    int reported = 0;

    /* 1. A pair of types has the same number of deals read in either
     *    direction, since it is the same set of deals counted twice. The
     *    mirroring depends on this. */
    int asymmetric = 0;
    for (int a = 0; a < NUM_HAND_TYPES; a++) {
        for (int b = 0; b < NUM_HAND_TYPES; b++) {
            if (deals[a * NUM_HAND_TYPES + b] != deals[b * NUM_HAND_TYPES + a])
                asymmetric++;
        }
    }
    if (asymmetric > 0) {
        fprintf(stderr, "  FAIL  %d cells disagree on their number of deals\n", asymmetric);
        all_passed = false;
    } else {
        fprintf(stderr, "  ok    deals are symmetric across all 28,561 cells\n");
    }

    /* 2. Every hand faces all 1,225 opponent hands, each counted once per
     *    concrete form of the hand itself. A missing or duplicated opponent
     *    hand shows up here. */
    int bad_rows = 0;
    for (int a = 0; a < NUM_HAND_TYPES; a++) {
        long long row = 0;
        for (int b = 0; b < NUM_HAND_TYPES; b++)
            row += deals[a * NUM_HAND_TYPES + b];

        if (row != (long long)types[a].combos * OPPONENT_HANDS)
            bad_rows++;
    }
    if (bad_rows > 0) {
        fprintf(stderr, "  FAIL  %d hands do not face exactly %d opponent hands\n",
                bad_rows, OPPONENT_HANDS);
        all_passed = false;
    } else {
        fprintf(stderr, "  ok    every hand faces all %d opponent hands\n", OPPONENT_HANDS);
    }

    /* 3. Wins, ties and losses must account for every board of every deal.
     *    This catches a cell that was never computed as surely as one that
     *    was computed twice. */
    int bad_cells = 0;
    for (int a = 0; a < NUM_HAND_TYPES; a++) {
        for (int b = 0; b < NUM_HAND_TYPES; b++) {
            const struct cell *cell = &matrix[a * NUM_HAND_TYPES + b];
            long long expected = deals[a * NUM_HAND_TYPES + b] * BOARDS_PER_MATCHUP;
            long long actual   = cell->wins + cell->ties + cell->losses;

            if (actual != expected) {
                bad_cells++;
                if (reported++ < 5)
                    fprintf(stderr, "  FAIL  %s vs %s: %lld boards counted, "
                            "expected %lld\n", types[a].label, types[b].label,
                            actual, expected);
            }
        }
    }
    if (bad_cells > 0) {
        fprintf(stderr, "  FAIL  %d cells do not account for all their boards\n", bad_cells);
        all_passed = false;
    } else {
        fprintf(stderr, "  ok    all 28,561 cells account for every board of every deal\n");
    }

    /* 4. A hand against itself must be worth exactly half the pot. Two hands
     *    of the same type are mirror images of each other, so whatever one
     *    wins the other wins equally often. This is the one cell per row that
     *    the mirroring does not produce, so it is a real check on the
     *    enumeration rather than a restatement of it. */
    int bad_diagonal = 0;
    for (int a = 0; a < NUM_HAND_TYPES; a++) {
        const struct cell *cell = &matrix[a * NUM_HAND_TYPES + a];
        if (cell->wins != cell->losses) {
            bad_diagonal++;
            if (bad_diagonal <= 3)
                fprintf(stderr, "  FAIL  %s vs %s: %lld wins but %lld losses\n",
                        types[a].label, types[a].label, cell->wins, cell->losses);
        }
    }
    if (bad_diagonal > 0) {
        fprintf(stderr, "  FAIL  %d hands are not worth half the pot against "
                        "themselves\n", bad_diagonal);
        all_passed = false;
    } else {
        fprintf(stderr, "  ok    every hand is worth exactly half the pot against itself\n");
    }

    /* 5. The whole matrix must average to exactly half the pot, because in a
     *    showdown between two random hands neither has an advantage.
     *
     *    In integers: equity is (2 x wins + ties) / (2 x total), so an average
     *    of exactly one half means the summed numerator equals the summed
     *    total. Written this way the check needs no division and no tolerance,
     *    and it constrains all 28,561 cells at once. */
    long long numerator = 0, total = 0;
    for (int i = 0; i < NUM_HAND_TYPES * NUM_HAND_TYPES; i++) {
        numerator += 2 * matrix[i].wins + matrix[i].ties;
        total     += matrix[i].wins + matrix[i].ties + matrix[i].losses;
    }
    if (numerator != total) {
        fprintf(stderr, "  FAIL  the matrix averages to %.12f, not exactly 1/2\n",
                (double)numerator / (2.0 * (double)total));
        all_passed = false;
    } else {
        fprintf(stderr, "  ok    the matrix averages to exactly 1/2 "
                        "(%lld = %lld, in integers)\n", numerator, total);
    }

    return all_passed;
}

static void write_matrix(FILE *out, const struct hand_type *types,
                         const struct cell *matrix, const long long *deals)
{
    fprintf(out, "# Exact head-to-head results for every ordered pair of the 169 "
                 "starting hands.\n");
    fprintf(out, "# Complete enumeration of all %ld boards per deal. No sampling: "
                 "these counts are exact.\n", BOARDS_PER_MATCHUP);
    fprintf(out, "hero,opponent,deals,boards,wins,ties,losses,equity\n");

    for (int a = 0; a < NUM_HAND_TYPES; a++) {
        for (int b = 0; b < NUM_HAND_TYPES; b++) {
            const struct cell *cell = &matrix[a * NUM_HAND_TYPES + b];
            long long boards = cell->wins + cell->ties + cell->losses;

            /* A head-to-head tie is always split two ways, so a tie is worth
             * half a board. */
            double equity = (2.0 * (double)cell->wins + (double)cell->ties)
                          / (2.0 * (double)boards);

            fprintf(out, "%s,%s,%lld,%lld,%lld,%lld,%lld,%.9f\n",
                    types[a].label, types[b].label,
                    deals[a * NUM_HAND_TYPES + b], boards,
                    cell->wins, cell->ties, cell->losses, equity);
        }
    }
}

/* Writes the exact equity of each hand against a single opponent holding a
 * uniformly random hand, which is one row of the matrix aggregated.
 *
 * This is the exact counterpart of what equity_mc.c estimates for one
 * opponent, and comparing the two is how the simulation is checked against
 * something that is not another simulation.
 */
static void write_versus_random(FILE *out, const struct hand_type *types,
                                const struct cell *matrix)
{
    fprintf(out, "# Exact equity of each starting hand against one opponent "
                 "holding a uniformly random hand.\n");
    fprintf(out, "# Aggregated from the exact head-to-head matrix, so these "
                 "figures are exact.\n");
    fprintf(out, "hand,family,combos,boards,wins,ties,losses,equity,"
                 "win_rate,tie_rate\n");

    for (int a = 0; a < NUM_HAND_TYPES; a++) {
        long long wins = 0, ties = 0, losses = 0;
        for (int b = 0; b < NUM_HAND_TYPES; b++) {
            const struct cell *cell = &matrix[a * NUM_HAND_TYPES + b];
            wins   += cell->wins;
            ties   += cell->ties;
            losses += cell->losses;
        }
        long long boards = wins + ties + losses;

        fprintf(out, "%s,%s,%d,%lld,%lld,%lld,%lld,%.9f,%.9f,%.9f\n",
                types[a].label, types[a].family, types[a].combos, boards,
                wins, ties, losses,
                (2.0 * (double)wins + (double)ties) / (2.0 * (double)boards),
                (double)wins / (double)boards,
                (double)ties / (double)boards);
    }
}

static int thread_count_default(void)
{
    const char *from_environment = getenv("THREADS");
    if (from_environment != NULL) {
        int requested = atoi(from_environment);
        if (requested > 0)
            return requested;
    }
    return 8;
}

int main(int argc, char **argv)
{
    /* In benchmark mode only the first N matchups are enumerated, to measure
     * the rate before committing to the full run. Nothing is written. */
    int benchmark_matchups = 0;
    const char *matrix_path = NULL;
    const char *versus_random_path = NULL;

    if (argc > 2 && strcmp(argv[1], "--benchmark") == 0) {
        benchmark_matchups = atoi(argv[2]);
    } else if (argc > 2) {
        matrix_path = argv[1];
        versus_random_path = argv[2];
    } else {
        fprintf(stderr,
                "usage: %s matrix.csv versus_random.csv\n"
                "       %s --benchmark N\n\n"
                "  Set THREADS to change the number of worker threads "
                "(default 8).\n", argv[0], argv[0]);
        return 1;
    }

    static struct hand_type types[NUM_HAND_TYPES];
    hand_types_all(types);

    struct matchup *matchups = malloc(sizeof *matchups * 120000);
    long long *deals = calloc(NUM_HAND_TYPES * NUM_HAND_TYPES, sizeof *deals);
    struct cell *matrix = calloc(NUM_HAND_TYPES * NUM_HAND_TYPES, sizeof *matrix);
    if (matchups == NULL || deals == NULL || matrix == NULL) {
        fprintf(stderr, "FAIL  out of memory\n");
        return 1;
    }

    int matchup_count = build_matchups(types, matchups, deals);

    fprintf(stderr, "Exact head-to-head matrix of the 169 starting hands\n");
    fprintf(stderr, "  %d cells, from %d matchups after suit isomorphism and "
                    "mirroring\n", NUM_HAND_TYPES * NUM_HAND_TYPES, matchup_count);
    fprintf(stderr, "  %ld boards each, %.0f billion hand evaluations\n",
            BOARDS_PER_MATCHUP,
            (double)matchup_count * BOARDS_PER_MATCHUP * 2 / 1e9);

    if (benchmark_matchups > 0 && benchmark_matchups < matchup_count)
        matchup_count = benchmark_matchups;

    int threads = thread_count_default();
    fprintf(stderr, "  %d threads\n\n", threads);

    struct job job = {
        .types = types,
        .matchups = matchups,
        .matchup_count = matchup_count,
    };
    atomic_init(&job.next_matchup, 0);
    atomic_init(&job.completed, 0);

    struct worker *workers = calloc((size_t)threads, sizeof *workers);
    pthread_t *handles = calloc((size_t)threads, sizeof *handles);

    for (int i = 0; i < threads; i++) {
        workers[i].job = &job;
        workers[i].matrix = calloc(NUM_HAND_TYPES * NUM_HAND_TYPES,
                                   sizeof *workers[i].matrix);
        if (workers[i].matrix == NULL) {
            fprintf(stderr, "FAIL  out of memory for worker %d\n", i);
            return 1;
        }
    }

    struct timespec started, finished;
    clock_gettime(CLOCK_MONOTONIC, &started);

    for (int i = 0; i < threads; i++)
        pthread_create(&handles[i], NULL, worker_run, &workers[i]);
    for (int i = 0; i < threads; i++)
        pthread_join(handles[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &finished);
    double seconds = (double)(finished.tv_sec - started.tv_sec)
                   + (double)(finished.tv_nsec - started.tv_nsec) / 1e9;

    /* Merge the private accumulators. */
    for (int i = 0; i < threads; i++) {
        for (int cell = 0; cell < NUM_HAND_TYPES * NUM_HAND_TYPES; cell++) {
            matrix[cell].wins   += workers[i].matrix[cell].wins;
            matrix[cell].ties   += workers[i].matrix[cell].ties;
            matrix[cell].losses += workers[i].matrix[cell].losses;
        }
        free(workers[i].matrix);
    }

    double evaluations = (double)matchup_count * BOARDS_PER_MATCHUP * 2;
    fprintf(stderr, "\nEnumerated %d matchups in %.1f s (%.0f million hand "
                    "evaluations per second)\n",
            matchup_count, seconds, evaluations / seconds / 1e6);

    if (benchmark_matchups > 0) {
        double full = (double)seconds / matchup_count * 47086;
        fprintf(stderr, "At this rate the full matrix takes %.1f minutes.\n",
                full / 60.0);
        return 0;
    }

    fprintf(stderr, "\nMirroring the lower triangle and validating:\n");
    mirror_lower_triangle(matrix);

    if (!validate(types, matrix, deals)) {
        fprintf(stderr, "\nValidation failed. No output written: this matrix is "
                        "wrong and must not be published.\n");
        return 1;
    }

    FILE *out = fopen(matrix_path, "w");
    if (out == NULL) {
        fprintf(stderr, "FAIL  cannot write %s\n", matrix_path);
        return 1;
    }
    write_matrix(out, types, matrix, deals);
    fclose(out);
    fprintf(stderr, "\nWrote %s\n", matrix_path);

    out = fopen(versus_random_path, "w");
    if (out == NULL) {
        fprintf(stderr, "FAIL  cannot write %s\n", versus_random_path);
        return 1;
    }
    write_versus_random(out, types, matrix);
    fclose(out);
    fprintf(stderr, "Wrote %s\n", versus_random_path);

    return 0;
}
