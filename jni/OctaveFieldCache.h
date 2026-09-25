#ifndef OCTAVE_FIELD_CACHE_H
#define OCTAVE_FIELD_CACHE_H

#include "cubiomes/biomenoise.h"
#include "cubiomes/noise.h"
#include <atomic>
#include <cstdint>
#include <vector>

/**
 * Period tables for Cont octA[0..4] (@contScale) and Ridge/Weirdness octA[0..2] (@weirdScale).
 * Sample lattice matches Cont/Weird prefilter centers: climate = i*(scale/4) + (scale/4)/2.
 */
struct OctaveACache {
    static constexpr int CONT_OCT = 5;
    static constexpr int RIDGE_OCT = 3;

    struct Field {
        int periodWorld = 0;
        int sampleScale = 0;
        int n = 0; /* periodWorld / sampleScale */
        std::vector<float> data; /* n * n, row-major */

        float atClimate(int bx, int bz) const;
    };

    Field contA[CONT_OCT];
    Field ridgeA[RIDGE_OCT];
    bool ready = false;
};

/** World-block periods for Cont A0..A4 and Ridge A0..A2. */
inline constexpr int kContAPeriodWorld[OctaveACache::CONT_OCT] = {
    524288, 262144, 131072, 65536, 32768
};
inline constexpr int kRidgeAPeriodWorld[OctaveACache::RIDGE_OCT] = {
    131072, 65536, 32768
};

/**
 * Build Cont+Ridge A period tables from seeded BiomeNoise.
 * Progress: current/total over all cells; phase should be 0. Honors pause/stop atomics.
 */
bool buildOctaveACache(
    OctaveACache *out,
    const BiomeNoise *bn,
    int contScale,
    int weirdScale,
    int numThreads,
    std::atomic_int *progressCurrent,
    std::atomic_int *progressTotal,
    const std::atomic_bool *tryPause,
    const std::atomic_bool *tryStop);

void setActiveOctaveACache(const OctaveACache *cache); /* nullptr disables */
const OctaveACache *getActiveOctaveACache();

#endif
