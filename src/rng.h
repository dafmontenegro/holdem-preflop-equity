/* rng.h — the random number generator used by the Monte Carlo estimates.
 *
 * WHY NOT rand()
 * --------------
 * A Monte Carlo equity estimate deals hundreds of millions of hands, and its
 * error bars are only meaningful if the draws really are independent and
 * uniform. The C standard library's rand() guarantees neither: its quality is
 * implementation-defined, and on some platforms it has as few as 15 bits of
 * state in the low bits. So the generator is spelled out here instead, with
 * both of the places where a careless implementation introduces bias handled
 * explicitly.
 *
 * THE GENERATOR: xoshiro256**
 * ---------------------------
 * A small, fast, well-tested generator (Blackman and Vigna) with 256 bits of
 * state and a period of 2^256 - 1. It passes the standard statistical test
 * suites, which rand() and the classic xorshift64 do not.
 *
 * It is not cryptographically secure, and does not need to be: nothing here
 * is adversarial, the requirement is uniformity, not unpredictability.
 *
 * SEEDING
 * -------
 * The state is filled from a seed with splitmix64, a separate generator whose
 * job is to turn one number into a well-mixed state. Seeding xoshiro's state
 * directly from a small number is the classic mistake: a state that is mostly
 * zeros produces a long stretch of poor output before it recovers.
 *
 * Every run takes its seed as an argument and prints it, so a result can be
 * reproduced exactly. Confidence intervals are not estimated from one seed:
 * a run reports the standard error computed from its own sample variance, and
 * separate seeds give independent samples to cross-check it against.
 */
#ifndef RNG_H
#define RNG_H

#include <stdint.h>

struct rng {
    uint64_t state[4];
};

/* splitmix64: turns a counter into well-mixed 64-bit values. Used only to
 * build the generator's initial state. */
static inline uint64_t splitmix64(uint64_t *counter)
{
    uint64_t z = (*counter += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static inline void rng_seed(struct rng *rng, uint64_t seed)
{
    uint64_t counter = seed;
    for (int i = 0; i < 4; i++)
        rng->state[i] = splitmix64(&counter);
}

static inline uint64_t rotate_left(uint64_t x, int bits)
{
    return (x << bits) | (x >> (64 - bits));
}

/* One draw: a uniform 64-bit value. */
static inline uint64_t rng_next(struct rng *rng)
{
    uint64_t *s = rng->state;
    uint64_t result = rotate_left(s[1] * 5, 7) * 9;

    uint64_t t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotate_left(s[3], 45);

    return result;
}

/* A uniform integer in [0, bound), with no modulo bias.
 *
 * WHY `rng_next() % bound` IS WRONG
 * ---------------------------------
 * 2^64 is not a multiple of most bounds, so the remainder favours the low
 * values. Drawing from 0..9 with a three-sided die shows the shape of it: the
 * first values come up more often than the last. The effect is tiny for the
 * bounds used here (at most 50), but it is a bias in the one direction that
 * matters, since the deck positions it skews are the cards being dealt, and
 * it costs nothing to remove.
 *
 * This is Lemire's method. Multiply the draw by the bound into a 128-bit
 * product: the high half is already a uniform value in [0, bound), and the
 * low half says whether this draw fell in the short, unfair tail of the
 * range. Only in that case, which is rarer than 1 in 10^17 for these bounds,
 * is another draw taken.
 */
static inline uint64_t rng_below(struct rng *rng, uint64_t bound)
{
    __uint128_t product = (__uint128_t)rng_next(rng) * bound;
    uint64_t low = (uint64_t)product;

    if (low < bound) {
        uint64_t threshold = (uint64_t)(-(__int128_t)bound) % bound;
        while (low < threshold) {
            product = (__uint128_t)rng_next(rng) * bound;
            low = (uint64_t)product;
        }
    }
    return (uint64_t)(product >> 64);
}

#endif /* RNG_H */
