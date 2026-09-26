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
 * Perlin-input-space period-256 tables for Cont/Ridge octB (one table each).
 * Lookup: (ax,az) = (bx*lf*f, bz*lf*f) mod 256, bilinear interp of samplePerlin,
 * then * amplitude (same order as sampleContOctB).
 */
struct OctaveBNoiseCache {
    static constexpr int CONT_OCT = 5;
    static constexpr int RIDGE_OCT = 3;
    static constexpr double kDoublePerlinF = 337.0 / 331.0;
    static constexpr double kPeriod = 256.0;

    struct Table {
        int n = 0;
        double amplitude = 1.0;
        double lacunarity = 1.0;
        std::vector<float> data; /* n*n raw samplePerlin (no amplitude) */

        double sampleRaw(double ax, double az) const;
        double sampleClimate(double bx, double bz) const;
    };

    Table contB[CONT_OCT];
    Table ridgeB[RIDGE_OCT];
    bool ready = false;
};

/** Dense table for Cont@32 / Weird@16 fidelity (~128 MiB for 8 tables). */
inline constexpr int kDefaultBNoiseTableN = 2048;

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

bool buildOctaveBNoiseCache(
    OctaveBNoiseCache *out,
    const BiomeNoise *bn,
    int tableN,
    int numThreads);

void setActiveOctaveACache(const OctaveACache *cache);
const OctaveACache *getActiveOctaveACache();

void setActiveOctaveBNoiseCache(const OctaveBNoiseCache *cache);
const OctaveBNoiseCache *getActiveOctaveBNoiseCache();

#endif
