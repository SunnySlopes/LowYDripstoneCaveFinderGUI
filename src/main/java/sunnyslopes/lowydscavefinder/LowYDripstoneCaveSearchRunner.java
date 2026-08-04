package sunnyslopes.lowydscavefinder;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.function.Consumer;

/**
 * Cave/river search via JNI ({@link LowYDripstoneCaveFinderBridge}). Progress from {@link LowYDripstoneCaveFinderBridge#getSearchProgress()}.
 */
public class LowYDripstoneCaveSearchRunner {

    public static final int RING_INNER = 24;
    public static final int RING_OUTER = 128;
    /** Used by UI for search area validation. */
    public static final int PHASE1_GRID_STEP = 16;

    /** Full ring area π×(128²−24²); upper bound for weighted total (s=) percentage. */
    public static final double RIVER_AREA_FULL = Math.PI * (128 * 128 - 24 * 24);

    /** Max search area (blocks). */
    public static final long MAX_SEARCH_AREA_BLOCKS = 60_000_001L * 60_000_001L;

    /** Default river weight in total = cave + river × weight. */
    public static final float DEFAULT_RIVER_WEIGHT = 0.7f;

    /** Matches native refinement threshold coefficient (opV); retained for JNI compatibility. */
    public static final float DEFAULT_PRESERVE_RANGE = 0.9f;

    /** Bucket size for "one maximum per region" output filtering. */
    public static final int OUTPUT_REGION_SIZE = 64;

    private static final long PROGRESS_POLL_MS = 100L;

    /** Deduplicate identical (x,z), then keep one point with maximum total per OUTPUT_REGION_SIZE square. */
    static List<int[]> filterResultsForOutput(int[] raw) {
        if (raw == null || raw.length < 5) {
            return List.of();
        }
        Map<Long, int[]> byXZ = new HashMap<>();
        for (int i = 0; i + 4 < raw.length; i += 5) {
            int x = raw[i], z = raw[i + 1], total = raw[i + 2];
            int cave = raw[i + 3], river = raw[i + 4];
            long pk = packXZ(x, z);
            int[] quint = { x, z, total, cave, river };
            byXZ.merge(pk, quint, (a, b) -> a[2] >= b[2] ? a : b);
        }
        Map<Long, int[]> byCell = new HashMap<>();
        for (int[] h : byXZ.values()) {
            int bx = Math.floorDiv(h[0], OUTPUT_REGION_SIZE);
            int bz = Math.floorDiv(h[1], OUTPUT_REGION_SIZE);
            long ck = packXZ(bx, bz);
            byCell.merge(ck, h, (a, b) -> a[2] >= b[2] ? a : b);
        }
        List<int[]> out = new ArrayList<>(byCell.values());
        out.sort(Comparator.comparingInt((int[] t) -> t[2]).reversed());
        return out;
    }

    private static long packXZ(int x, int z) {
        return ((long) x << 32) ^ (z & 0xffffffffL);
    }

    /**
     * @param done when true, search has finished (success, stop, or error); UI should re-enable controls.
     * @param nativePauseSettled when {@code true}, native pause has taken effect (from {@link LowYDripstoneCaveFinderBridge#getSearchProgress()}).
     * @param nativeStopRequested native stop flag is set (search may still be winding down until {@code done}).
     */
    public record ProgressInfo(int phase, long processed, long total, double percentage, long elapsedMs, long remainingMs,
                               int currentSeedIndex, int totalSeeds, boolean done, boolean refiningResults,
                               boolean nativePauseSettled, boolean nativeStopRequested) {
        public static ProgressInfo of(int phase, long processed, long total, long elapsedMs, long remainingMs) {
            return of(phase, processed, total, elapsedMs, remainingMs, false, false, false);
        }

        /** @param refiningResults native phase 2 (post-scan refinement); UI may show non-numeric message. */
        public static ProgressInfo of(int phase, long processed, long total, long elapsedMs, long remainingMs,
                                      boolean refiningResults) {
            return of(phase, processed, total, elapsedMs, remainingMs, refiningResults, false, false);
        }

        public static ProgressInfo of(int phase, long processed, long total, long elapsedMs, long remainingMs,
                                      boolean refiningResults, boolean nativePauseSettled, boolean nativeStopRequested) {
            double pct = total > 0 ? (processed * 100.0 / total) : 0;
            return new ProgressInfo(phase, processed, total, pct, elapsedMs, remainingMs, 0, 0, false, refiningResults,
                nativePauseSettled, nativeStopRequested);
        }

        /** Final callback after native {@code riverSearch} returns. */
        public static ProgressInfo terminal(long elapsedMs, long processed, long total) {
            long t = Math.max(1, total);
            long p = Math.min(processed, t);
            double pct = t > 0 ? (p * 100.0 / t) : 100.0;
            return new ProgressInfo(0, p, t, pct, elapsedMs, 0, 0, 0, true, false, false, false);
        }
    }

    /**
     * Whether native code has finished winding down into pause (async pause). Uses {@link LowYDripstoneCaveFinderBridge#getSearchProgress()}.
     */
    public static boolean isNativePauseSettled(int[] progress) {
        if (progress == null || progress.length < 4) {
            return false;
        }
        boolean tryPause = progress.length > 4 && progress[4] != 0;
        if (!tryPause) {
            return false;
        }
        int phase = progress[2];
        int status = progress[3];
        if (phase == 1) {
            return status == 1;
        }
        if (phase == 2) {
            return true;
        }
        return true;
    }

    private static final Object NATIVE_LOCK = new Object();
    private static volatile LowYDripstoneCaveSearchRunner activeRunner;

    /** True while any runner still holds the native search lock (including wind-down after stop). */
    public static boolean isAnySearchActive() {
        synchronized (NATIVE_LOCK) {
            return activeRunner != null;
        }
    }

    private volatile boolean isRunning = false;
    private volatile boolean isPaused = false;
    private volatile long runStartTimeMs = 0;
    private volatile long totalPausedMs = 0;
    private volatile long pauseStartMs = 0;

    public boolean isRunning() { return isRunning; }
    public boolean isPaused() { return isPaused; }

    public void pause() {
        if (!isPaused) {
            isPaused = true;
            pauseStartMs = System.currentTimeMillis();
            try {
                LowYDripstoneCaveFinderBridge.pause();
            } catch (Throwable ignored) {
            }
        }
    }

    public void resume() {
        if (isPaused) {
            if (pauseStartMs > 0) {
                totalPausedMs += System.currentTimeMillis() - pauseStartMs;
                pauseStartMs = 0;
            }
            isPaused = false;
            try {
                LowYDripstoneCaveFinderBridge.resume();
            } catch (Throwable ignored) {
            }
        }
    }

    /** Requests native stop; {@link #isRunning()} stays true until {@code riverSearch} returns on the search thread. */
    public void stop() {
        try {
            LowYDripstoneCaveFinderBridge.stop();
        } catch (Throwable ignored) {
        }
    }

    private long getElapsedMs() {
        long base = System.currentTimeMillis() - runStartTimeMs;
        long paused = totalPausedMs;
        if (pauseStartMs > 0) {
            paused += System.currentTimeMillis() - pauseStartMs;
        }
        return Math.max(0, base - paused);
    }

    /** Start river search for one seed. Runs in background thread. Returns false if another search is still active. */
    public boolean startRiverSearch(long seed, int minX, int maxX, int minZ, int maxZ, int minArea,
                                 float riverWeight, int threadCount,
                                 Consumer<ProgressInfo> progressCallback, Consumer<String> resultCallback) {
        if (!tryAcquireActiveRunner()) {
            return false;
        }
        Thread t = new Thread(() -> runRiverSearch(seed, minX, maxX, minZ, maxZ, minArea, riverWeight, threadCount, progressCallback, resultCallback),
            "lowydripstonecavefinder-search");
        t.setDaemon(true);
        t.start();
        return true;
    }

    /** Run river search for one seed on the current thread (for list search). Returns false if another search is still active. */
    public boolean runRiverSearchBlocking(long seed, int minX, int maxX, int minZ, int maxZ, int minArea,
                                       float riverWeight, int threadCount,
                                       Consumer<ProgressInfo> progressCallback, Consumer<String> resultCallback) {
        if (!tryAcquireActiveRunner()) {
            return false;
        }
        runRiverSearch(seed, minX, maxX, minZ, maxZ, minArea, riverWeight, threadCount, progressCallback, resultCallback);
        return true;
    }

    private boolean tryAcquireActiveRunner() {
        synchronized (NATIVE_LOCK) {
            if (activeRunner != null) {
                return false;
            }
            activeRunner = this;
            return true;
        }
    }

    private void releaseActiveRunner() {
        synchronized (NATIVE_LOCK) {
            if (activeRunner == this) {
                activeRunner = null;
            }
        }
    }

    private void runRiverSearch(long seed, int minX, int maxX, int minZ, int maxZ, int minArea,
                               float riverWeight, int threadCount,
                               Consumer<ProgressInfo> progressCallback, Consumer<String> resultCallback) {
        isRunning = true;
        isPaused = false;
        totalPausedMs = 0;
        pauseStartMs = 0;
        runStartTimeMs = System.currentTimeMillis();
        int threads = Math.max(1, Math.min(threadCount, Runtime.getRuntime().availableProcessors()));

        long searchWidth = (long) (maxX - minX) + 1;
        long searchHeight = (long) (maxZ - minZ) + 1;
        long searchArea = searchWidth * searchHeight;
        if (searchArea <= 0 || searchArea > MAX_SEARCH_AREA_BLOCKS) {
            isRunning = false;
            releaseActiveRunner();
            if (resultCallback != null && searchArea > MAX_SEARCH_AREA_BLOCKS) {
                resultCallback.accept("[Error] Search area too large (" + searchArea + " blocks). Must not exceed "
                    + MAX_SEARCH_AREA_BLOCKS + " (400M blocks).");
            }
            if (progressCallback != null) {
                progressCallback.accept(ProgressInfo.terminal(getElapsedMs(), 0, 1));
            }
            return;
        }

        int startX = minX - RING_OUTER;
        int startZ = minZ - RING_OUTER;
        int width = (maxX - minX + 1) + 2 * RING_OUTER;
        int height = (maxZ - minZ + 1) + 2 * RING_OUTER;

        AtomicInteger phase1TotalMax = new AtomicInteger(0);
        AtomicLong lastProcessed = new AtomicLong(0);
        AtomicLong lastTotal = new AtomicLong(0);

        ScheduledExecutorService progressScheduler = Executors.newSingleThreadScheduledExecutor(r -> {
            Thread t = new Thread(r, "lowydripstonecavefinder-progress");
            t.setDaemon(true);
            return t;
        });

        progressScheduler.scheduleAtFixedRate(() -> {
            if (progressCallback == null || !isRunning || activeRunner != LowYDripstoneCaveSearchRunner.this) {
                return;
            }
            try {
                int[] arr = LowYDripstoneCaveFinderBridge.getSearchProgress();
                if (arr == null || arr.length < 4) {
                    return;
                }
                boolean tryStop = arr.length > 5 && arr[5] != 0;
                boolean pauseSettled = isNativePauseSettled(arr);

                int cur = arr[0];
                int tot = arr[1];
                int nPhase = arr[2];

                long processed;
                long total;
                boolean refining;
                if (nPhase == 1) {
                    if (tot <= 1) {
                        return;
                    }
                    phase1TotalMax.set(Math.max(phase1TotalMax.get(), tot));
                    processed = cur;
                    total = tot;
                    refining = false;
                } else if (nPhase == 2) {
                    int p1 = phase1TotalMax.get();
                    if (p1 <= 0) {
                        processed = cur;
                        total = Math.max(tot, 1L);
                    } else {
                        processed = (long) p1 + cur;
                        total = (long) p1 + Math.max(tot, 1);
                    }
                    refining = true;
                } else if (nPhase == -1) {
                    processed = lastProcessed.get();
                    total = Math.max(Math.max(lastTotal.get(), processed), 1L);
                    refining = true;
                } else {
                    return;
                }

                lastProcessed.set(processed);
                lastTotal.set(total);

                long elapsed = getElapsedMs();
                long remaining = 0L;
                if (!isPaused && !refining && total > processed && processed > 0) {
                    // Use double to avoid long overflow on huge full-world totals
                    double eta = elapsed * ((double) (total - processed) / (double) processed);
                    if (eta > 0 && eta < (double) Long.MAX_VALUE) {
                        remaining = (long) eta;
                    }
                }
                progressCallback.accept(ProgressInfo.of(0, processed, total, elapsed, remaining, refining, pauseSettled, tryStop));
            } catch (Throwable ignored) {
            }
        }, PROGRESS_POLL_MS, PROGRESS_POLL_MS, TimeUnit.MILLISECONDS);

        try {
            float weight = Math.max(0f, Math.min(1f, riverWeight));
            int[] raw;
            synchronized (NATIVE_LOCK) {
                raw = LowYDripstoneCaveFinderBridge.riverSearch(seed, startX, startZ, width, height, 0, minArea,
                    DEFAULT_PRESERVE_RANGE, weight, threads);
            }

            if (raw != null && resultCallback != null) {
                int fullAreaInt = (int) RIVER_AREA_FULL;
                StringBuilder sb = new StringBuilder();
                for (int[] row : filterResultsForOutput(raw)) {
                    int x = row[0], z = row[1], total = row[2], cave = row[3], river = row[4];
                    double ratioPercent = total / RIVER_AREA_FULL * 100.0;
                    sb.append(String.format("/tp %d 64 %d s=%d/%d = %.2f%% cave=%d river=%d",
                        x, z, total, fullAreaInt, ratioPercent, cave, river))
                        .append('\n');
                }
                if (sb.length() > 0) {
                    resultCallback.accept(sb.toString());
                }
            }
        } catch (UnsatisfiedLinkError e) {
            if (resultCallback != null) {
                resultCallback.accept("[Error] Native library not loaded. Build with: jni/build-jni.bat. " + e.getMessage());
            }
        } finally {
            progressScheduler.shutdownNow();
            try {
                progressScheduler.awaitTermination(2, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
            isRunning = false;
            releaseActiveRunner();
            if (pauseStartMs > 0) {
                totalPausedMs += System.currentTimeMillis() - pauseStartMs;
                pauseStartMs = 0;
            }
            isPaused = false;
            if (progressCallback != null) {
                long p = lastProcessed.get();
                long t = lastTotal.get();
                if (t <= 0) {
                    t = Math.max(p, 1);
                }
                progressCallback.accept(ProgressInfo.terminal(getElapsedMs(), p, t));
            }
        }
    }
}
