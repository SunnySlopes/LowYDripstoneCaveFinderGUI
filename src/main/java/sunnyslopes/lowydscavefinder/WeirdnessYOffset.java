package sunnyslopes.lowydscavefinder;

/**
 * Host-side weirdfinder y-offset score.
 * score = |yo0A-0.5| + |yo1A-0.5| + |yo0B-0.5| + |yo1B-0.5|,
 * yo = top 24 bits of the octave fork * 2^-24.
 */
public final class WeirdnessYOffset {
    /** Single-seed searches whose block area is at least this run the luck check. */
    public static final long MIN_SEARCH_AREA = 6000001L * 6000001L;
    /** Scores strictly below this use the normal lucky prompt. */
    public static final float LUCKY_BELOW = 0.75f;
    /** Scores strictly below this still prompt, and say the chance is smaller. */
    public static final float LUCKY_LOW_PROB_BELOW = 0.8f;

    private static final long XRSR_MIX1 = 0xbf58476d1ce4e5b9L;
    private static final long XRSR_MIX2 = 0x94d049bb133111ebL;
    private static final long XRSR_SILVER = 0x6a09e667f3bcc909L;
    private static final long XRSR_GOLDEN = 0x9e3779b97f4a7c15L;
    private static final long XRSR_FALLBACK_LO = -7046029254386353131L;
    private static final long XRSR_FALLBACK_HI = 7640891576956012809L;

    /** md5 "minecraft:weirdness" */
    private static final long HASH_W_LO = 0xefc8ef4d36102b34L;
    private static final long HASH_W_HI = 0x1beeeb324a0f24eaL;
    /** md5 "octave_-7" / "octave_-6" */
    private static final long OCT_M7_LO = 0xf11268128982754fL;
    private static final long OCT_M7_HI = 0x257a1d670430b0aaL;
    private static final long OCT_M6_LO = 0xe51c98ce7d1de664L;
    private static final long OCT_M6_HI = 0x5f9478a733040c45L;

    private WeirdnessYOffset() {
    }

    public static float score(long seed) {
        Rng world = new Rng(seed);
        long[] noise = forkPair(world);
        noise[0] ^= HASH_W_LO;
        noise[1] ^= HASH_W_HI;
        Rng noiseRng = new Rng(noise[0], noise[1]);
        long[] a = noiseYoFork(forkPair(noiseRng));
        long[] b = noiseYoFork(forkPair(noiseRng));
        long[] h0 = advanceHash(OCT_M7_LO, OCT_M7_HI);
        long[] h1 = advanceHash(OCT_M6_LO, OCT_M6_HI);
        float w0a = octaveYo(a, h0);
        float w1a = octaveYo(a, h1);
        float w0b = octaveYo(b, h0);
        float w1b = octaveYo(b, h1);
        return Math.abs(w0a - 0.5f) + Math.abs(w1a - 0.5f) + Math.abs(w0b - 0.5f) + Math.abs(w1b - 0.5f);
    }

    private static long rol64(long a, int bits) {
        return (a << bits) | (a >>> (64 - bits));
    }

    private static long mix64(long a) {
        a = (a ^ (a >>> 30)) * XRSR_MIX1;
        a = (a ^ (a >>> 27)) * XRSR_MIX2;
        return a ^ (a >>> 31);
    }

    private static final class Rng {
        long lo;
        long hi;

        Rng(long seed) {
            seed ^= XRSR_SILVER;
            lo = mix64(seed);
            hi = mix64(seed + XRSR_GOLDEN);
            if ((lo | hi) == 0L) {
                lo = XRSR_FALLBACK_LO;
                hi = XRSR_FALLBACK_HI;
            }
        }

        Rng(long lo, long hi) {
            this.lo = lo;
            this.hi = hi;
            if ((this.lo | this.hi) == 0L) {
                this.lo = XRSR_FALLBACK_LO;
                this.hi = XRSR_FALLBACK_HI;
            }
        }

        long next() {
            long l = lo;
            long h = hi;
            long r = rol64(l + h, 17) + l;
            h ^= l;
            lo = rol64(l, 49) ^ h ^ (h << 21);
            hi = rol64(h, 28);
            return r;
        }
    }

    private static long[] forkPair(Rng rng) {
        return new long[]{rng.next(), rng.next()};
    }

    private static long[] advanceHash(long lo, long hi) {
        Rng rng = new Rng(lo, hi);
        rng.next();
        return new long[]{rng.lo, rng.hi};
    }

    private static long[] noiseYoFork(long[] fork) {
        Rng rng = new Rng(fork[0], fork[1]);
        rng.next();
        return new long[]{rng.lo, rng.hi};
    }

    private static float octaveYo(long[] yo, long[] hash) {
        Rng rng = new Rng(yo[0] ^ hash[0], yo[1] ^ hash[1]);
        int top = (int) (rng.next() >>> 32) & 0xFFFFFF;
        return top * 5.9604645E-8f;
    }
}
