package sunnyslopes.lowydscavefinder;

import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

class FilterResultsForOutputTest {

    @Test
    void emptyOrTooShortReturnsEmpty() {
        assertTrue(LowYDripstoneCaveSearchRunner.filterResultsForOutput(null).isEmpty());
        assertTrue(LowYDripstoneCaveSearchRunner.filterResultsForOutput(new int[] { 1, 2, 3, 4 }).isEmpty());
    }

    @Test
    void deduplicatesSameXZKeepingMaxTotal() {
        int[] raw = {
            100, 200, 50, 10, 20,
            100, 200, 80, 30, 40,
        };
        List<int[]> out = LowYDripstoneCaveSearchRunner.filterResultsForOutput(raw);
        assertEquals(1, out.size());
        assertEquals(100, out.get(0)[0]);
        assertEquals(200, out.get(0)[1]);
        assertEquals(80, out.get(0)[2]);
        assertEquals(30, out.get(0)[3]);
        assertEquals(40, out.get(0)[4]);
    }

    @Test
    void keepsOneMaxTotalPerOutputRegion() {
        int region = LowYDripstoneCaveSearchRunner.OUTPUT_REGION_SIZE;
        int[] raw = {
            0, 0, 100, 10, 20,
            region, 0, 90, 5, 15,
            0, region, 70, 8, 12,
        };
        List<int[]> out = LowYDripstoneCaveSearchRunner.filterResultsForOutput(raw);
        assertEquals(3, out.size());
        out.sort((a, b) -> Integer.compare(b[2], a[2]));
        assertEquals(100, out.get(0)[2]);
        assertEquals(90, out.get(1)[2]);
        assertEquals(70, out.get(2)[2]);
    }

    @Test
    void mergesNearbyPointsIntoOnePer64Cell() {
        int[] raw = {
            0, 0, 100, 10, 20,
            10, 10, 90, 5, 15,
            64, 64, 50, 3, 4,
        };
        List<int[]> out = LowYDripstoneCaveSearchRunner.filterResultsForOutput(raw);
        assertEquals(2, out.size());
    }

    @Test
    void sortsByTotalDescending() {
        int[] raw = {
            0, 0, 30, 1, 2,
            64, 64, 50, 3, 4,
            128, 128, 40, 5, 6,
        };
        List<int[]> out = LowYDripstoneCaveSearchRunner.filterResultsForOutput(raw);
        assertEquals(3, out.size());
        assertEquals(50, out.get(0)[2]);
        assertEquals(40, out.get(1)[2]);
        assertEquals(30, out.get(2)[2]);
    }
}
