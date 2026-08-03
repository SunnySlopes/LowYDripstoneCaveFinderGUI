#ifndef BIOME_SAMPLER_H
#define BIOME_SAMPLER_H

#include "cubiomes/biomenoise.h"
#include "cubiomes/generator.h"

static constexpr int PRECISE_Y_COUNT = 3;
extern const int PRECISE_YS[PRECISE_Y_COUNT];

static constexpr uint32_t COARSE_SAMPLE_FLAGS = SAMPLE_NO_SHIFT | SAMPLE_NO_DEPTH;
static constexpr int COARSE_Y_PARAM = 256 / 4 + 1;
/** Match legacy full-C gate: (int)(10000*C) > 5500 ⇒ C > 0.55 */
static constexpr double CONT_PARTIAL_FINAL_THRESHOLD = 0.55;

bool passCaveWeirdness(const BiomeNoise *bn, int bx, int bz, uint32_t sample_flags);
bool passCaveClimate(const BiomeNoise *bn, int bx, int bz, uint32_t sample_flags);
bool passContinentalnessPartial(const BiomeNoise *bn, int bx, int bz);
bool passCoarseCaveCell(const BiomeNoise *bn, int bx, int bz, uint32_t sample_flags);
void samplePreciseCell(const Generator *g, int worldX, int worldZ, int *riverHits, int *caveHits);

#endif
