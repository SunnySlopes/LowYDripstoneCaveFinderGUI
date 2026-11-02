package sunnyslopes.lowydscavefinder;

/**
 * Host-side thresholds and page-jump decision inputs for the weirdness
 * y-offset score.
 *
 * <p>The score itself is computed by the native
 * {@code LowYDripstoneCaveFinderBase} submodule through
 * {@link LowYDripstoneCaveFinderBridge#weirdnessYOffset(long)}. This class
 * keeps only the constants the GUI uses to decide whether to offer or perform
 * the fast-search page jump.
 */
public final class WeirdnessYOffset {
    /** Single-seed searches whose block area is at least this run the luck check. */
    public static final long MIN_SEARCH_AREA = 6000001L * 6000001L;
    /** First-tab scores strictly below this offer the fast-search prompt. */
    public static final float LUCKY_BELOW = 0.75f;
    /** Fast-search scores strictly above this warn that the search will likely come out nothing. */
    public static final float LUCKY_LOW_PROB_BELOW = 0.85f;

    private WeirdnessYOffset() {
    }

    /**
     * Convenience wrapper around the base submodule's single native entry
     * point. Threshold comparison and navigation stay in the GUI.
     */
    public static float score(long seed) {
        return LowYDripstoneCaveFinderBridge.weirdnessYOffset(seed);
    }
}
