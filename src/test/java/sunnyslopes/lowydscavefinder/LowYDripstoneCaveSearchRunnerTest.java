package sunnyslopes.lowydscavefinder;

import java.util.function.Consumer;

/**
 * Runnable test for cave/river search: run this class's main() to see progress and results in the console.
 */
public class LowYDripstoneCaveSearchRunnerTest {

    public static void main(String[] args) {
        long seed = -8180004378910677489L;
        int minX = 65536, maxX = 200000, minZ = -200000, maxZ = -65536;
        int minArea = 40000;

        LowYDripstoneCaveSearchRunner runner = new LowYDripstoneCaveSearchRunner();
        Consumer<LowYDripstoneCaveSearchRunner.ProgressInfo> onProgress = info -> {
            if (info.done()) {
                System.out.printf("[完成] %d / %d (%.1f%%) 已用 %d ms%n",
                    info.processed(), info.total(), info.percentage(), info.elapsedMs());
                return;
            }
            System.out.printf("进度 %d / %d (%.1f%%) 已用 %d ms%n",
                info.processed(), info.total(), info.percentage(), info.elapsedMs());
        };
        Consumer<String> onResult = line -> {
            if (line.startsWith("[Error]")) {
                System.err.println("Result: " + line);
            } else {
                System.out.println("Result: " + line);
            }
        };

        System.out.println("Starting cave search: seed=" + seed + " range [" + minX + "," + maxX + "] x [" + minZ + "," + maxZ + "] minArea=" + minArea);
        int threadCount = Runtime.getRuntime().availableProcessors();
        runner.runRiverSearchBlocking(seed, minX, maxX, minZ, maxZ, minArea, LowYDripstoneCaveSearchRunner.DEFAULT_RIVER_WEIGHT, threadCount, onProgress, onResult);
        System.out.println("Done.");
    }
}
