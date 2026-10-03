/* beaten.c — how many opponents hold a hand that beats mine.
 *
 * THE QUESTION, AND WHY IT NEEDS A DEFINITION FIRST
 * -------------------------------------------------
 * "What are the chances somebody has a better hand than me?" is the question
 * every player actually asks preflop, and before the flop it does not mean
 * anything on its own. Hand strength is only decided once five board cards
 * are out: A-A is not "better than" 7-6 suited in the way that a flush is
 * better than a pair, it is merely a heavy favourite. Asked literally, the
 * question has no answer.
 *
 * So it is given one, and this is the definition everything below rests on:
 *
 *     An opponent hand BEATS mine when my exact equity against that specific
 *     hand is below one half.
 *
 * In other words: if the two hands were turned face up and dealt out to the
 * river every possible way, I would collect less than half the pot. This is a
 * choice, and it is worth seeing what it does and does not say. It is a
 * statement about the long run against that one hand, not a claim that the
 * opponent will win this pot. It ignores how far below one half: a hand I
 * beat 49.9% of the time counts the same as one I beat 20% of the time, which
 * is why this program also reports the whole distribution of equity rather
 * than the classification alone. And it treats the boundary exactly: because
 * equity is the rational number (2*wins + ties) / (2*boards), "below one
 * half" is the integer test
 *
 *     2*wins + ties < boards
 *
 * with no rounding and no tolerance. Hands that land exactly on one half are
 * counted in their own category rather than silently pushed to one side; they
 * are real, and they are the hands that are mirror images of mine, such as
 * AcKc against AdKd.
 *
 * WHAT MAKES THE MULTIPLE-OPPONENT VERSION HARD
 * ---------------------------------------------
 * Against one opponent the answer is a count: how many of the 1,225 hands an
 * opponent can hold beat mine. Against n opponents it is not, and the reason
 * is the reason this program exists.
 *
 * The opponents' hands are not independent. They come out of one deck, so the
 * moment one opponent holds the ace of spades no other can, and hands that
 * beat me tend to use the same cards as each other. The familiar shortcut
 *
 *     P(at least one of n beats me) = 1 - (1 - p)^n
 *
 * assumes independence and is therefore wrong. One of this program's jobs is
 * to say exactly how wrong, by computing the right answer next to it.
 *
 * The right answer requires counting, among all the ways to deal 2n cards
 * into n hands, how many give exactly k hands that beat me. That is a
 * counting problem over *disjoint* hands, and it is genuinely hard: every
 * hand is a pair of cards, so a set of n disjoint hands is a matching of size
 * n in a graph on the 50 remaining cards. Counting matchings of a given size
 * in a general graph is a hard problem, and at n = 8 there are on the order of
 * 10^17 of them, so enumeration is out of the question.
 *
 * HOW FAR EXACTNESS REACHES, AND WHERE IT STOPS
 * ---------------------------------------------
 * Exact for n = 1 to 4. Estimated, with standard errors, for n = 5 to 8. The
 * boundary is not a matter of patience; it is where the method runs out, and
 * the next section says why.
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
#include "suits.h"

#define OPPONENT_HANDS   1225      /* C(50,2) */
#define MAX_EXACT        4         /* opponents the exact method reaches */
#define MAX_OPPONENTS    8
#define MAX_DISJOINT     (MAX_EXACT)

/* One opponent hand, as seen by one particular hero hand. */
struct opponent {
    int cards[2];
    int hero_equity_numerator;   /* 2*wins + ties, as a count of half-boards */
    long long boards;
    bool beats_hero;
    bool splits_with_hero;
};

/* Everything known about one hero hand's 1,225 opponent hands. */
struct landscape {
    int hero_type;
    struct opponent opponents[OPPONENT_HANDS];
    int count;

    int beating;      /* opponent hands with equity above one half for them */
    int splitting;    /* opponent hands worth exactly one half */
    int beaten;       /* opponent hands the hero is a favourite against */

    /* The beating hands, as a graph on the 50 remaining cards: `beating_pairs`
     * lists them, `degree[c]` is how many of them use card c, and
     * `is_beating[c][d]` answers "is this pair one of them" in one step.
     * Everything in the exact counting below is phrased in these terms. */
    int beating_pairs[OPPONENT_HANDS][2];
    int degree[NUM_CARDS];
    bool is_beating[NUM_CARDS][NUM_CARDS];
};

/* ------------------------------------------------------------------------
 * Reading the exact matchup results
 * ------------------------------------------------------------------------ */

/* Builds each hero hand's view of all 1,225 opponent hands, from the exact
 * per-matchup results written by headsup.c.
 *
 * That file holds one row per *orbit* of opponent hands, not one per hand:
 * opponent hands that renaming the unused suits maps onto each other have
 * identical equity against the hero, so they share a row. Expanding the file
 * back out to 1,225 hands per hero means recomputing which orbit each hand
 * falls into, which is done with the same `suits.h` routines that produced
 * the file, so the two cannot drift apart.
 */
static bool load_landscapes(const char *path, const struct hand_type *types,
                            struct landscape *landscapes)
{
    /* Equity numerator per (hero type, orbit key). The orbit key is a pair of
     * cards packed into one number, so the table is indexed by it directly. */
    static long long numerator[NUM_HAND_TYPES][NUM_CARDS * NUM_CARDS];
    static long long boards[NUM_HAND_TYPES][NUM_CARDS * NUM_CARDS];
    static bool present[NUM_HAND_TYPES][NUM_CARDS * NUM_CARDS];

    FILE *in = fopen(path, "r");
    if (in == NULL) {
        fprintf(stderr, "FAIL  cannot read %s\n", path);
        return false;
    }

    char line[512];
    int rows = 0;
    while (fgets(line, sizeof line, in) != NULL) {
        if (line[0] == '#' || strncmp(line, "hero,", 5) == 0)
            continue;

        char hero_label[8], opponent_cards[8], opponent_label[8];
        int stands_for;
        long long row_boards, wins, ties, losses;

        if (sscanf(line, "%7[^,],%7[^,],%7[^,],%d,%lld,%lld,%lld,%lld",
                   hero_label, opponent_cards, opponent_label, &stands_for,
                   &row_boards, &wins, &ties, &losses) != 8)
            continue;

        int hero_type = -1;
        for (int i = 0; i < NUM_HAND_TYPES; i++) {
            if (strcmp(types[i].label, hero_label) == 0) {
                hero_type = i;
                break;
            }
        }
        if (hero_type < 0) {
            fprintf(stderr, "FAIL  unknown hand \"%s\" in %s\n", hero_label, path);
            fclose(in);
            return false;
        }

        int cards[2];
        if (cards_parse(opponent_cards, cards, 2) != 2) {
            fprintf(stderr, "FAIL  cannot read opponent cards \"%s\"\n", opponent_cards);
            fclose(in);
            return false;
        }

        int stabiliser[NUM_SUIT_PERMUTATIONS];
        int stabiliser_size = suit_stabiliser(types[hero_type].cards[0],
                                              types[hero_type].cards[1], stabiliser);
        int key = hand_canonical_key(cards[0], cards[1], stabiliser, stabiliser_size);

        numerator[hero_type][key] = 2 * wins + ties;
        boards[hero_type][key] = row_boards;
        present[hero_type][key] = true;
        rows++;
    }
    fclose(in);

    fprintf(stderr, "  read %d matchup rows\n", rows);

    /* Expand to every opponent hand and classify it. */
    for (int hero_type = 0; hero_type < NUM_HAND_TYPES; hero_type++) {
        const struct hand_type *hero = &types[hero_type];
        struct landscape *landscape = &landscapes[hero_type];

        memset(landscape, 0, sizeof *landscape);
        landscape->hero_type = hero_type;

        int stabiliser[NUM_SUIT_PERMUTATIONS];
        int stabiliser_size = suit_stabiliser(hero->cards[0], hero->cards[1],
                                              stabiliser);

        for (int first = 0; first < NUM_CARDS; first++) {
            if (first == hero->cards[0] || first == hero->cards[1])
                continue;
            for (int second = first + 1; second < NUM_CARDS; second++) {
                if (second == hero->cards[0] || second == hero->cards[1])
                    continue;

                int key = hand_canonical_key(first, second, stabiliser, stabiliser_size);
                if (!present[hero_type][key]) {
                    fprintf(stderr, "FAIL  %s has no result against %d,%d\n",
                            hero->label, first, second);
                    return false;
                }

                struct opponent *opponent = &landscape->opponents[landscape->count++];
                opponent->cards[0] = first;
                opponent->cards[1] = second;
                opponent->boards = boards[hero_type][key];
                opponent->hero_equity_numerator = (int)numerator[hero_type][key];

                /* The definition, as an integer comparison: the hero's equity
                 * is (2*wins + ties) / (2*boards), so it is below one half
                 * exactly when 2*wins + ties is below boards. */
                opponent->beats_hero =
                    numerator[hero_type][key] < boards[hero_type][key];
                opponent->splits_with_hero =
                    numerator[hero_type][key] == boards[hero_type][key];

                if (opponent->beats_hero) {
                    int index = landscape->beating;
                    landscape->beating_pairs[index][0] = first;
                    landscape->beating_pairs[index][1] = second;
                    landscape->beating++;

                    landscape->degree[first]++;
                    landscape->degree[second]++;
                    landscape->is_beating[first][second] = true;
                    landscape->is_beating[second][first] = true;
                } else if (opponent->splits_with_hero) {
                    landscape->splitting++;
                } else {
                    landscape->beaten++;
                }
            }
        }

        if (landscape->count != OPPONENT_HANDS) {
            fprintf(stderr, "FAIL  %s faces %d opponent hands, expected %d\n",
                    hero->label, landscape->count, OPPONENT_HANDS);
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------------
 * The exact count, for 1 to 4 opponents
 * ------------------------------------------------------------------------ */

/* How many beating hands use none of the cards in `used`.
 *
 * Counted by complement, which is what makes it cheap. A beating hand touches
 * `used` if either of its cards is in there, so
 *
 *     touching = sum over c in used of degree[c] - pairs entirely inside used
 *
 * The subtraction is there because a beating hand with *both* cards in `used`
 * gets counted twice by the degree sum, once for each of its cards. Nothing
 * can be counted more than twice, since a hand has only two cards, so one
 * subtraction is the whole correction.
 *
 * The cost is the square of the size of `used`, never the number of beating
 * hands, which is what lets this sit in the innermost loop below.
 */
static int beating_hands_avoiding(const struct landscape *landscape,
                                  const int *used, int used_count)
{
    int touching = 0;

    for (int i = 0; i < used_count; i++)
        touching += landscape->degree[used[i]];

    for (int i = 0; i < used_count; i++) {
        for (int j = i + 1; j < used_count; j++) {
            if (landscape->is_beating[used[i]][used[j]])
                touching--;
        }
    }
    return landscape->beating - touching;
}

/* Counts the sets of 1, 2, 3 and 4 beating hands that can be dealt at once,
 * that is, that share no card. Results go into `sets[1..4]`.
 *
 * WHY THESE FOUR NUMBERS ARE THE WHOLE PROBLEM
 * --------------------------------------------
 * The inclusion-exclusion below needs exactly one input: for each j, how many
 * ways j of the opponents can *all* hold beating hands. That is the number of
 * ways to pick j beating hands that are pairwise card-disjoint, times the j!
 * ways to hand them out to j named opponents. Everything else in the
 * calculation is plain combinatorics on the cards that are left.
 *
 * HOW THEY ARE COUNTED
 * --------------------
 * Not by enumerating the sets, which would be hopeless at size 4 and beyond,
 * but by one recurrence applied three times. Take every disjoint set of size
 * j, count the beating hands that avoid all the cards it uses, and add those
 * counts up. Each disjoint set of size j+1 is reached exactly j+1 times that
 * way, once from each of its subsets of size j, so dividing by j+1 gives the
 * number of sets of size j+1:
 *
 *     sets[j+1] = (1 / (j+1)) * sum over disjoint j-sets of (hands avoiding it)
 *
 * The three sums are accumulated in a single triple loop, each level adding
 * one hand and skipping any that collides with the cards already used. The
 * loops only ever reach size 3, and the count at size 4 comes out of the
 * recurrence rather than from a fourth loop.
 *
 * WHERE THIS STOPS, AND WHY IT IS NOT A MATTER OF PATIENCE
 * -------------------------------------------------------
 * Reaching sets[5] would mean enumerating disjoint sets of size 4, and the
 * loop nest grows by a factor of the number of beating hands each time: with
 * up to 1,200 of them, size 3 is about 300 million steps per hand and size 4
 * would be about 90 billion, for each of the 169 starting hands. That is why
 * five opponents and up are estimated instead. The obstacle is real and not
 * an implementation detail: counting disjoint sets of a given size is
 * counting matchings of that size in a graph, and no efficient method for
 * that is known.
 */
static void count_disjoint_sets(const struct landscape *landscape,
                                long long *sets)
{
    long long reached_from_size1 = 0;   /* counts size-2 sets, twice each   */
    long long reached_from_size2 = 0;   /* counts size-3 sets, three times  */
    long long reached_from_size3 = 0;   /* counts size-4 sets, four times   */

    int count = landscape->beating;

    for (int i = 0; i < count; i++) {
        int used1[6];
        used1[0] = landscape->beating_pairs[i][0];
        used1[1] = landscape->beating_pairs[i][1];

        reached_from_size1 += beating_hands_avoiding(landscape, used1, 2);

        for (int j = i + 1; j < count; j++) {
            int a = landscape->beating_pairs[j][0];
            int b = landscape->beating_pairs[j][1];
            if (a == used1[0] || a == used1[1] || b == used1[0] || b == used1[1])
                continue;

            int used2[6] = { used1[0], used1[1], a, b };
            reached_from_size2 += beating_hands_avoiding(landscape, used2, 4);

            for (int k = j + 1; k < count; k++) {
                int c = landscape->beating_pairs[k][0];
                int d = landscape->beating_pairs[k][1];
                bool collides = false;
                for (int u = 0; u < 4; u++) {
                    if (c == used2[u] || d == used2[u]) {
                        collides = true;
                        break;
                    }
                }
                if (collides)
                    continue;

                int used3[6] = { used2[0], used2[1], used2[2], used2[3], c, d };
                reached_from_size3 += beating_hands_avoiding(landscape, used3, 6);
            }
        }
    }

    sets[0] = 1;
    sets[1] = count;
    sets[2] = reached_from_size1 / 2;
    sets[3] = reached_from_size2 / 3;
    sets[4] = reached_from_size3 / 4;
}

/* The number of ways to deal `hands` two-card hands, in order, from `cards`
 * cards: C(cards,2) x C(cards-2,2) x ... Hands are ordered because the
 * opponents are distinct people. */
static long long ordered_deals(int hands, int cards)
{
    long long ways = 1;

    for (int i = 0; i < hands; i++) {
        int remaining = cards - 2 * i;
        ways *= (long long)remaining * (remaining - 1) / 2;
    }
    return ways;
}

static long long binomial(int n, int k)
{
    if (k < 0 || k > n)
        return 0;

    long long result = 1;
    for (int i = 0; i < k; i++)
        result = result * (n - i) / (i + 1);
    return result;
}

/* The exact distribution of how many of `opponents` hold a beating hand.
 *
 * Writes the number of deals giving exactly k beating hands into
 * `deals_with_k[0..opponents]`.
 *
 * THE DERIVATION, IN FULL
 * -----------------------
 * Start with something easier to count than "exactly k". For a *named* group
 * of j opponents, count the deals in which all j of them hold beating hands,
 * with the other opponents holding whatever they like:
 *
 *     (ways to give those j opponents disjoint beating hands)
 *       x (ways to deal the remaining opponents from the cards still unused)
 *     = j! * sets[j] * ordered_deals(opponents - j, 50 - 2j)
 *
 * Add that over all C(opponents, j) ways of choosing which j opponents, and
 * call the total S(j). Every deal with exactly m beating hands is counted in
 * S(j) once for each of its C(m, j) groups of j beating opponents, so
 *
 *     S(j) = sum over m >= j of C(m, j) * N(m)
 *
 * where N(m) is what we want. That triangular system inverts into
 *
 *     N(k) = sum over j >= k of (-1)^(j-k) * C(j, k) * S(j)
 *
 * which is the standard binomial inversion, and the loop below is exactly
 * that formula. Nothing is approximated anywhere: every term is an integer
 * count of deals, and the alternating signs cancel exactly.
 *
 * Note that the "others hold whatever they like" step is what keeps this from
 * needing the far harder count of sets that mix beating and non-beating
 * hands. The price is the alternating sum, which is free.
 */
static void exact_distribution(const long long *sets, int opponents,
                               long long *deals_with_k)
{
    long long s[MAX_EXACT + 1];

    for (int j = 0; j <= opponents; j++) {
        long long arrangements = 1;
        for (int i = 2; i <= j; i++)
            arrangements *= i;                      /* j! */

        s[j] = binomial(opponents, j) * arrangements * sets[j]
             * ordered_deals(opponents - j, 50 - 2 * j);
    }

    for (int k = 0; k <= opponents; k++) {
        long long total = 0;
        for (int j = k; j <= opponents; j++) {
            long long term = binomial(j, k) * s[j];
            total += ((j - k) % 2 == 0) ? term : -term;
        }
        deals_with_k[k] = total;
    }
}

/* ------------------------------------------------------------------------
 * An independent check of the exact count
 * ------------------------------------------------------------------------ */

/* Counts the same distribution by walking every possible deal.
 *
 * This exists to check the inclusion-exclusion above, and it checks it in the
 * only way that means anything: by sharing none of its reasoning. There is no
 * recurrence here and no alternating sum, just every way to deal two or three
 * hands, classified one at a time. If the two agree on all 169 starting
 * hands, the derivation is right.
 *
 * It is only usable for two and three opponents. Two opponents is about
 * 750,000 deals per starting hand and runs instantly; three is 300 million
 * and runs in seconds, so it is checked on a few hands rather than all 169.
 * Four would be 90 billion per hand, which is the same wall the exact method
 * hits one step later, and is the reason the exact method is needed at all.
 */
static void brute_force_distribution(const struct landscape *landscape,
                                     int opponents, long long *deals_with_k)
{
    int count = landscape->count;

    for (int k = 0; k <= opponents; k++)
        deals_with_k[k] = 0;

    /* Hands are picked in index order and the result multiplied by the number
     * of orderings, since the opponents are distinct but the classification
     * does not care who holds what. */
    long long orderings = 1;
    for (int i = 2; i <= opponents; i++)
        orderings *= i;

    for (int i = 0; i < count; i++) {
        const struct opponent *first = &landscape->opponents[i];

        for (int j = i + 1; j < count; j++) {
            const struct opponent *second = &landscape->opponents[j];
            if (second->cards[0] == first->cards[0] ||
                second->cards[0] == first->cards[1] ||
                second->cards[1] == first->cards[0] ||
                second->cards[1] == first->cards[1])
                continue;

            if (opponents == 2) {
                int beating = (int)first->beats_hero + (int)second->beats_hero;
                deals_with_k[beating] += orderings;
                continue;
            }

            for (int k = j + 1; k < count; k++) {
                const struct opponent *third = &landscape->opponents[k];
                if (third->cards[0] == first->cards[0] ||
                    third->cards[0] == first->cards[1] ||
                    third->cards[0] == second->cards[0] ||
                    third->cards[0] == second->cards[1] ||
                    third->cards[1] == first->cards[0] ||
                    third->cards[1] == first->cards[1] ||
                    third->cards[1] == second->cards[0] ||
                    third->cards[1] == second->cards[1])
                    continue;

                int beating = (int)first->beats_hero + (int)second->beats_hero
                            + (int)third->beats_hero;
                deals_with_k[beating] += orderings;
            }
        }
    }
}

/* ------------------------------------------------------------------------
 * The estimate, for 5 to 8 opponents
 * ------------------------------------------------------------------------ */

/* Simulates deals and counts how many opponents hold a beating hand.
 *
 * Writes the fraction of trials giving exactly k into `probability[0..opponents]`.
 *
 * Each trial shuffles just enough of the 50 remaining cards to deal every
 * opponent, which is a partial Fisher-Yates pass, and then reads the hands
 * off in pairs. Nothing here assumes the hands are independent: they are
 * dealt from one deck, exactly as the exact method counts them, so the only
 * difference between this and the exact answer is sampling error, which is
 * reported alongside.
 */
static void simulate_distribution(const struct landscape *landscape,
                                  const struct hand_type *hero,
                                  int opponents, long long trials,
                                  struct rng *rng, double *probability)
{
    int deck[50];
    int deck_size = deck_without(hero->cards, deck);
    int cards_needed = 2 * opponents;

    long long trials_with_k[MAX_OPPONENTS + 1] = { 0 };

    for (long long trial = 0; trial < trials; trial++) {
        for (int i = 0; i < cards_needed; i++) {
            int j = i + (int)rng_below(rng, (uint64_t)(deck_size - i));
            int swap = deck[i];
            deck[i] = deck[j];
            deck[j] = swap;
        }

        int beating = 0;
        for (int o = 0; o < opponents; o++) {
            if (landscape->is_beating[deck[2 * o]][deck[2 * o + 1]])
                beating++;
        }
        trials_with_k[beating]++;
    }

    for (int k = 0; k <= opponents; k++)
        probability[k] = (double)trials_with_k[k] / (double)trials;
}

/* ------------------------------------------------------------------------
 * The shape of the equity distribution
 * ------------------------------------------------------------------------ */

/* Upper edges of the bands the equity distribution is reported in, as
 * percentages. The band boundaries are placed where the meaning changes:
 * below 20 is "dominated", 45 to 55 is "a coin flip", above 80 is "crushing".
 * The two bands either side of 50 are narrow on purpose, because that is
 * where the beats-me classification draws its line and the reader deserves to
 * see how many hands sit right next to it. */
#define NUM_BANDS 8
static const int BAND_UPPER_PERCENT[NUM_BANDS] = { 20, 35, 45, 50, 55, 65, 80, 100 };

/* Counts how many of the 1,225 opponent hands fall in each band.
 *
 * The comparison is exact. The hero's equity against one opponent hand is
 * numerator / (2 * boards), so it is at most t percent exactly when
 * numerator * 100 <= t * 2 * boards, which is integer arithmetic throughout:
 * no hand is put in the wrong band by a rounding error, least of all at the
 * 50% edge where it would matter most.
 */
static void count_bands(const struct landscape *landscape, int *bands)
{
    for (int band = 0; band < NUM_BANDS; band++)
        bands[band] = 0;

    for (int i = 0; i < landscape->count; i++) {
        const struct opponent *opponent = &landscape->opponents[i];
        long long scaled = (long long)opponent->hero_equity_numerator * 100;

        for (int band = 0; band < NUM_BANDS; band++) {
            long long edge = (long long)BAND_UPPER_PERCENT[band] * 2 * opponent->boards;
            if (scaled <= edge) {
                bands[band]++;
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------------
 * Output
 * ------------------------------------------------------------------------ */

static double equity_of(const struct opponent *opponent)
{
    return (double)opponent->hero_equity_numerator / (2.0 * (double)opponent->boards);
}

static int compare_equity(const void *left, const void *right)
{
    double a = equity_of(left), b = equity_of(right);
    return (a > b) - (a < b);
}

static void write_landscape(FILE *out, const struct hand_type *types,
                            struct landscape *landscapes)
{
    fprintf(out, "# For each starting hand, the shape of its equity against all "
                 "1,225 hands an opponent can hold.\n");
    fprintf(out, "# Exact: every figure is a count over complete board "
                 "enumeration, not a sample.\n");
    fprintf(out, "# \"beats_hero\" means the hero's equity against that one hand is "
                 "below one half.\n");
    fprintf(out, "hand,family,combos,opponent_hands,beats_hero,splits_with_hero,"
                 "hero_favourite,lowest_equity,median_equity,highest_equity");
    for (int band = 0; band < NUM_BANDS; band++)
        fprintf(out, ",upto_%d_percent", BAND_UPPER_PERCENT[band]);
    fprintf(out, "\n");

    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        struct landscape *landscape = &landscapes[i];

        qsort(landscape->opponents, (size_t)landscape->count,
              sizeof landscape->opponents[0], compare_equity);

        int bands[NUM_BANDS];
        count_bands(landscape, bands);

        fprintf(out, "%s,%s,%d,%d,%d,%d,%d,%.9f,%.9f,%.9f",
                types[i].label, types[i].family, types[i].combos,
                landscape->count, landscape->beating, landscape->splitting,
                landscape->beaten,
                equity_of(&landscape->opponents[0]),
                equity_of(&landscape->opponents[landscape->count / 2]),
                equity_of(&landscape->opponents[landscape->count - 1]));

        for (int band = 0; band < NUM_BANDS; band++)
            fprintf(out, ",%d", bands[band]);
        fprintf(out, "\n");
    }
}

/* Writes the full distribution of k, and the headline summary next to the
 * shortcut it replaces. */
static void write_distributions(FILE *detail, FILE *summary,
                                const struct hand_type *types,
                                const struct landscape *landscapes,
                                long long trials, unsigned long long seed)
{
    fprintf(detail, "# How many of n opponents hold a hand that beats yours.\n");
    fprintf(detail, "# Exact by inclusion-exclusion for 1 to %d opponents; "
                    "estimated by simulation for %d to %d\n",
            MAX_EXACT, MAX_EXACT + 1, MAX_OPPONENTS);
    fprintf(detail, "# (%lld trials, seed %llu), because counting disjoint sets "
                    "of 5 beating hands is not feasible.\n", trials, seed);
    fprintf(detail, "hand,opponents,beaten_by,probability,standard_error,method\n");

    fprintf(summary, "# The chance that at least one of n opponents holds a hand "
                     "that beats yours, against\n");
    fprintf(summary, "# the common shortcut 1-(1-p)^n, which assumes the "
                     "opponents' hands are independent.\n");
    fprintf(summary, "# They are not: the hands come out of one deck. "
                     "\"error_pp\" is how far the shortcut is off,\n");
    fprintf(summary, "# in percentage points.\n");
    fprintf(summary, "hand,family,opponents,at_least_one,standard_error,"
                     "independent_approximation,error_pp,method\n");

    struct rng rng;
    rng_seed(&rng, seed);

    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        const struct landscape *landscape = &landscapes[i];

        long long sets[MAX_EXACT + 1];
        count_disjoint_sets(landscape, sets);

        /* The chance one opponent holds a beating hand, which is all the
         * independent shortcut knows. */
        double single = (double)landscape->beating / (double)OPPONENT_HANDS;

        for (int opponents = 1; opponents <= MAX_OPPONENTS; opponents++) {
            double probability[MAX_OPPONENTS + 1] = { 0 };
            double errors[MAX_OPPONENTS + 1] = { 0 };
            const char *method;

            if (opponents <= MAX_EXACT) {
                long long deals_with_k[MAX_EXACT + 1];
                exact_distribution(sets, opponents, deals_with_k);

                long long total = ordered_deals(opponents, 50);
                for (int k = 0; k <= opponents; k++)
                    probability[k] = (double)deals_with_k[k] / (double)total;
                method = "exact";
            } else {
                simulate_distribution(landscape, &types[i], opponents,
                                      trials, &rng, probability);
                for (int k = 0; k <= opponents; k++)
                    errors[k] = sqrt(probability[k] * (1.0 - probability[k])
                                     / (double)trials);
                method = "montecarlo";
            }

            for (int k = 0; k <= opponents; k++)
                fprintf(detail, "%s,%d,%d,%.9f,%.9f,%s\n",
                        types[i].label, opponents, k, probability[k],
                        errors[k], method);

            double at_least_one = 1.0 - probability[0];
            double approximation = 1.0 - pow(1.0 - single, opponents);

            fprintf(summary, "%s,%s,%d,%.9f,%.9f,%.9f,%+.4f,%s\n",
                    types[i].label, types[i].family, opponents,
                    at_least_one, errors[0], approximation,
                    (approximation - at_least_one) * 100.0, method);
        }
    }
}

/* ------------------------------------------------------------------------
 * Validation
 * ------------------------------------------------------------------------ */

static bool validate(const struct hand_type *types, const struct landscape *landscapes)
{
    bool all_passed = true;

    /* 1. Every hand must face all 1,225 opponent hands, and the three
     *    classifications must account for all of them. */
    int bad = 0;
    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        const struct landscape *landscape = &landscapes[i];
        if (landscape->beating + landscape->splitting + landscape->beaten
            != OPPONENT_HANDS)
            bad++;
    }
    if (bad > 0) {
        fprintf(stderr, "  FAIL  %d hands do not classify all %d opponent hands\n",
                bad, OPPONENT_HANDS);
        all_passed = false;
    } else {
        fprintf(stderr, "  ok    every hand classifies all %d opponent hands\n",
                OPPONENT_HANDS);
    }

    /* 2. The exact distribution must account for every possible deal. This
     *    catches arithmetic overflow in the alternating sum, which would
     *    otherwise produce plausible-looking probabilities. */
    bad = 0;
    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        long long sets[MAX_EXACT + 1];
        count_disjoint_sets(&landscapes[i], sets);

        for (int opponents = 1; opponents <= MAX_EXACT; opponents++) {
            long long deals_with_k[MAX_EXACT + 1];
            exact_distribution(sets, opponents, deals_with_k);

            long long total = 0;
            for (int k = 0; k <= opponents; k++) {
                if (deals_with_k[k] < 0)
                    bad++;
                total += deals_with_k[k];
            }
            if (total != ordered_deals(opponents, 50))
                bad++;
        }
    }
    if (bad > 0) {
        fprintf(stderr, "  FAIL  %d exact distributions do not account for every "
                        "deal, or went negative\n", bad);
        all_passed = false;
    } else {
        fprintf(stderr, "  ok    every exact distribution accounts for all "
                        "%lld deals and stays non-negative\n",
                ordered_deals(MAX_EXACT, 50));
    }

    /* 3. The inclusion-exclusion must agree with brute force, which shares
     *    none of its reasoning. Two opponents is checked on all 169 hands;
     *    three on a sample, because it is 300 million deals per hand. */
    bad = 0;
    for (int i = 0; i < NUM_HAND_TYPES; i++) {
        long long sets[MAX_EXACT + 1];
        count_disjoint_sets(&landscapes[i], sets);

        long long derived[MAX_EXACT + 1], counted[MAX_EXACT + 1];
        exact_distribution(sets, 2, derived);
        brute_force_distribution(&landscapes[i], 2, counted);

        for (int k = 0; k <= 2; k++) {
            if (derived[k] != counted[k]) {
                if (bad < 3)
                    fprintf(stderr, "  FAIL  %s, 2 opponents, k=%d: "
                            "inclusion-exclusion %lld, brute force %lld\n",
                            types[i].label, k, derived[k], counted[k]);
                bad++;
            }
        }
    }
    if (bad > 0) {
        fprintf(stderr, "  FAIL  the two methods disagree on %d values\n", bad);
        all_passed = false;
    } else {
        fprintf(stderr, "  ok    inclusion-exclusion matches brute force for 2 "
                        "opponents, on all %d hands\n", NUM_HAND_TYPES);
    }

    /* The same check at three opponents, on a spread of hands: the strongest,
     * the weakest, a middling one and a pair. */
    const char *sampled[] = { "AA", "98s", "A5o", "32o" };
    bad = 0;
    for (size_t s = 0; s < sizeof sampled / sizeof sampled[0]; s++) {
        int index = -1;
        for (int i = 0; i < NUM_HAND_TYPES; i++) {
            if (strcmp(types[i].label, sampled[s]) == 0) {
                index = i;
                break;
            }
        }
        if (index < 0)
            continue;

        long long sets[MAX_EXACT + 1];
        count_disjoint_sets(&landscapes[index], sets);

        long long derived[MAX_EXACT + 1], counted[MAX_EXACT + 1];
        exact_distribution(sets, 3, derived);
        brute_force_distribution(&landscapes[index], 3, counted);

        for (int k = 0; k <= 3; k++) {
            if (derived[k] != counted[k]) {
                fprintf(stderr, "  FAIL  %s, 3 opponents, k=%d: "
                        "inclusion-exclusion %lld, brute force %lld\n",
                        sampled[s], k, derived[k], counted[k]);
                bad++;
            }
        }
    }
    if (bad > 0) {
        all_passed = false;
    } else {
        fprintf(stderr, "  ok    and for 3 opponents on AA, 98s, A5o and 32o "
                        "(300 million deals each)\n");
    }

    return all_passed;
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr,
                "usage: %s matchups.csv landscape.csv by_k.csv at_least_one.csv "
                "[trials] [seed]\n\n"
                "  matchups.csv     the exact per-matchup results from headsup\n"
                "  landscape.csv    the shape of each hand's equity distribution\n"
                "  by_k.csv         how many opponents hold a beating hand\n"
                "  at_least_one.csv the headline table, against the independent "
                "shortcut\n"
                "  trials           simulation trials per cell for 5 to 8 "
                "opponents (default 200000)\n"
                "  seed             seed for the random generator (default 1)\n",
                argv[0]);
        return 1;
    }

    const char *matchups_path = argv[1];
    const char *landscape_path = argv[2];
    const char *by_k_path = argv[3];
    const char *at_least_one_path = argv[4];
    long long trials = (argc > 5) ? atoll(argv[5]) : 200000;
    unsigned long long seed = (argc > 6) ? strtoull(argv[6], NULL, 10) : 1;

    static struct hand_type types[NUM_HAND_TYPES];
    hand_types_all(types);

    static struct landscape landscapes[NUM_HAND_TYPES];

    fprintf(stderr, "How many opponents hold a hand that beats yours\n");
    if (!load_landscapes(matchups_path, types, landscapes))
        return 1;

    fprintf(stderr, "\nValidating:\n");
    clock_t started = clock();
    if (!validate(types, landscapes)) {
        fprintf(stderr, "\nValidation failed. No output written.\n");
        return 1;
    }
    fprintf(stderr, "  (%.1f s)\n",
            (double)(clock() - started) / CLOCKS_PER_SEC);

    FILE *landscape_file = fopen(landscape_path, "w");
    FILE *by_k_file = fopen(by_k_path, "w");
    FILE *at_least_one_file = fopen(at_least_one_path, "w");
    if (landscape_file == NULL || by_k_file == NULL || at_least_one_file == NULL) {
        fprintf(stderr, "FAIL  cannot write the output files\n");
        return 1;
    }

    fprintf(stderr, "\nWriting the distributions");
    if (trials > 0)
        fprintf(stderr, " (%lld trials per estimated cell, seed %llu)", trials, seed);
    fprintf(stderr, "\n");

    write_distributions(by_k_file, at_least_one_file, types, landscapes, trials, seed);
    fclose(by_k_file);
    fclose(at_least_one_file);
    fprintf(stderr, "Wrote %s\nWrote %s\n", by_k_path, at_least_one_path);

    write_landscape(landscape_file, types, landscapes);
    fclose(landscape_file);
    fprintf(stderr, "Wrote %s\n", landscape_path);

    return 0;
}
