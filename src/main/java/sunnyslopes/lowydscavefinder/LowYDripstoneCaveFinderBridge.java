package sunnyslopes.lowydscavefinder;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;

/**
 * JNI bridge to the native LowYDripstoneCaveFinderBase library.
 *
 * <p>All C++/cubiomes computation lives in the {@code LowYDripstoneCaveFinderBase}
 * submodule (Gradle project {@code :lowydripstonecavefinderbase}). This class is
 * the Java side of the native contract:
 * <ul>
 *   <li>{@link #riverSearch} returns 5 ints per hit
 *       ({@code x, z, total, cave, river}) or {@code null};</li>
 *   <li>{@link #getSearchProgress} returns
 *       {@code [current, total, phase, status, tryPause, tryStop]} or {@code null};</li>
 *   <li>{@link #weirdnessYOffset} is the single, separate entry point for the
 *       seed score. The GUI keeps the threshold comparison and the page-jump
 *       decision.</li>
 * </ul>
 * The native header checked into
 * {@code LowYDripstoneCaveFinderBase/jni/generated/sunnyslopes_lowydscavefinder_LowYDripstoneCaveFinderBridge.h}
 * must match this file; the main module's {@code verifyJniContract} task
 * enforces that.
 *
 * <p>The search samples every layer in {@code [yMin, yMax]} and reports the
 * per-layer average volume, so the weighted total keeps the 0..49659 scale
 * regardless of the layer count.  River area is weighted by
 * {@code riverWeight} into total.
 */
public final class LowYDripstoneCaveFinderBridge {

    private static boolean loaded = false;

    static {
        loadNativeLibrary();
    }

    private static void loadNativeLibrary() {
        if (loaded) return;
        UnsatisfiedLinkError firstError = null;

        // Prefer a freshly built DLL over stale System.loadLibrary / JAR copies.
        if (tryLoadFromProjectNativeDirs()) {
            loaded = true;
            return;
        }
        try {
            System.loadLibrary("libDripstoneCaveFinderLibJ");
            loaded = true;
            return;
        } catch (UnsatisfiedLinkError e) {
            firstError = e;
        }
        if (tryLoadFromBundledResources()) {
            loaded = true;
            return;
        }
        throw new UnsatisfiedLinkError(
            "DripstoneCaveFinder native library not found. Build with: gradlew buildNative "
                + "(which delegates to :lowydripstonecavefinderbase:copyNativeArtifacts). "
                + (firstError != null ? firstError.getMessage() : ""));
    }

    private static boolean tryLoadFromProjectNativeDirs() {
        Path base = Paths.get(System.getProperty("user.dir", "."));
        List<String> libNames = getNativeLibNamesForCurrentOs();
        String osFolder = getOsResourceFolder();
        Path[] candidates = {
            base.resolve("native"),
            base.resolve("build").resolve("native"),
            // LowYDripstoneCaveFinderBase submodule build output.
            base.resolve("LowYDripstoneCaveFinderBase").resolve("build").resolve("native"),
            base.resolve("build").resolve("generated").resolve("native-resources")
                .resolve("native").resolve(osFolder != null ? osFolder : "windows"),
            base.resolve("build"),
        };
        for (Path dir : candidates) {
            for (String libName : libNames) {
                File f = dir.resolve(libName).toFile();
                if (f.isFile()) {
                    try {
                        System.load(f.getAbsolutePath());
                        return true;
                    } catch (UnsatisfiedLinkError ignored) {
                        // try the next candidate
                    }
                }
            }
        }
        return false;
    }

    private static boolean tryLoadFromBundledResources() {
        String osFolder = getOsResourceFolder();
        if (osFolder == null) return false;

        for (String libName : getNativeLibNamesForCurrentOs()) {
            String resourcePath = "/native/" + osFolder + "/" + libName;
            if (tryLoadSingleBundledResource(resourcePath, libName)) {
                return true;
            }
        }
        return false;
    }

    private static boolean tryLoadSingleBundledResource(String resourcePath, String libName) {
        try (InputStream is = LowYDripstoneCaveFinderBridge.class.getResourceAsStream(resourcePath)) {
            if (is == null) return false;
            Path tempDir = Files.createTempDirectory("lowydripstonecavefinder-native-");
            Path tempLib = tempDir.resolve(libName);
            Files.copy(is, tempLib, StandardCopyOption.REPLACE_EXISTING);
            tempLib.toFile().deleteOnExit();
            tempDir.toFile().deleteOnExit();
            System.load(tempLib.toAbsolutePath().toString());
            return true;
        } catch (IOException | UnsatisfiedLinkError ex) {
            return false;
        }
    }

    private static String getOsResourceFolder() {
        String os = System.getProperty("os.name", "").toLowerCase();
        if (os.contains("win")) return "windows";
        if (os.contains("mac")) return "macos";
        if (os.contains("nix") || os.contains("nux") || os.contains("aix") || os.contains("linux")) return "linux";
        return null;
    }

    private static List<String> getNativeLibNamesForCurrentOs() {
        String os = System.getProperty("os.name", "").toLowerCase();
        List<String> names = new ArrayList<>();
        if (os.contains("win")) {
            names.add("libDripstoneCaveFinderLibJ.dll");
            names.add("DripstoneCaveFinderLibJ.dll");
        } else if (os.contains("mac")) {
            names.add("libDripstoneCaveFinderLibJ.dylib");
            names.add("DripstoneCaveFinderLibJ.dylib");
        } else {
            names.add("libDripstoneCaveFinderLibJ.so");
            names.add("DripstoneCaveFinderLibJ.so");
        }
        return names;
    }

    /**
     * Volume-integrated cave/river search over an explicit layer window.
     *
     * @param seed world seed
     * @param startX expanded native window start X (user minX - RING_OUTER)
     * @param startZ expanded native window start Z (user minZ - RING_OUTER)
     * @param width expanded native window width (user width + 2 * RING_OUTER)
     * @param height expanded native window height (user height + 2 * RING_OUTER)
     * @param yMin lowest sampled layer (>= -63)
     * @param yMax highest sampled layer (<= 319); {@code yMax - yMin + 1 <= 48}
     * @param coarseY coarse-stage recall height (carried for protocol compatibility;
     *     the climate prefilter of this engine is Y-independent)
     * @param afkY AFK / player Y of the shell model; the scored region is the 3D
     *     shell {@code 24 < distance <= 128} around {@code (x, afkY, z)}.
     *     Every cave/river block inside the shell counts.
     * @param minArea minimum weighted total area
     * @param preserveRange phase-1 refinement coefficient (0.9 keeps the historical behaviour)
     * @param riverWeight river area weight in total = cave + river * riverWeight, range [0, 1]
     * @param numThreads thread count ({@code <= 0} uses hardware concurrency in native)
     * @param mcVersion Minecraft version string for cubiomes (e.g. {@code "26.1"}, {@code "26.3"})
     * @param contScale phase-1 continentalness prefilter grid scale (e.g. 128 fast / 32 precise)
     * @param weirdScale phase-1 weirdness ring grid scale (e.g. 32 fast / 16 precise)
     * @param searchMode 0 fast grid, 1 precise grid, 2 full-map large-pit chains
     * @param allowGpu {@code true} lets the native side use the OpenCL backend when the
     *     window is large enough and a usable device exists; {@code false} never loads
     *     OpenCL. An unusable device is silent: the CPU path yields the same results.
     * @return [x1, z1, total1, cave1, river1, x2, ...] or {@code null}
     */
    public static native int[] riverSearch(long seed, int startX, int startZ,
        int width, int height, int yMin, int yMax, int coarseY, int afkY, int minArea, float preserveRange,
        float riverWeight, int numThreads, String mcVersion, int contScale, int weirdScale,
        int searchMode, boolean allowGpu);

    /**
     * Progress query. Returns {@code [current, total, phase, status, tryPause, tryStop]} or {@code null} if idle.
     * {@code phase}: 1 = phase-1 scan, 2 = refinement, -1 = finishing.
     * {@code status}: phase 1: 0 = chunks active, 1 = no chunk in flight; phase 2: 1 = refining, 2 = refinement step done.
     * {@code tryPause} / {@code tryStop}: 1 if native pause/stop was requested (async; use with {@code status} for settled pause).
     */
    public static native int[] getSearchProgress();

    /**
     * Returns now results as {@code [x1, z1, total1, cave1, river1, ...]} (sorted), or {@code null} if not implemented.
     */
    public static native int[] getNowResult();

    /**
     * Try pause task.
     */
    public static native boolean pause();

    /**
     * Try resume task.
     */
    public static native boolean resume();

    /**
     * Try to stop the task.
     */
    public static native boolean stop();

    /**
     * Weirdness y-offset score, computed by the base submodule.
     *
     * <p>score = |yo0A-0.5| + |yo1A-0.5| + |yo0B-0.5| + |yo1B-0.5|,
     * yo = top 24 bits of the octave fork * 2^-24. The GUI owns the
     * {@code 0.75} / {@code 0.85} threshold comparison and the page jump.
     *
     * @param seed world seed
     * @return score in {@code [0, 2]}
     */
    public static native float weirdnessYOffset(long seed);

    private LowYDripstoneCaveFinderBridge() {}
}
