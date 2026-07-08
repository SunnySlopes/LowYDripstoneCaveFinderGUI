#include "BiomeSampler.h"
#include "cubiomes/biomes.h"
#include "cubiomes/noise.h"
#include <cstdlib>

const int PRECISE_YS[PRECISE_Y_COUNT] = { -60, -56, -52 };

static int quantWeirdness(const BiomeNoise *bn, int bx, int bz, uint32_t sample_flags)
{
    double px = bx, pz = bz;
    if (!(sample_flags & SAMPLE_NO_SHIFT))
    {
        px += sampleDoublePerlin(&bn->climate[NP_SHIFT], bx, 0, bz) * 4.0;
        pz += sampleDoublePerlin(&bn->climate[NP_SHIFT], bz, bx, 0) * 4.0;
    }
    float w = sampleDoublePerlin(&bn->climate[NP_WEIRDNESS], px, 0, pz);
    int wi = (int) (10000.0 * w);
    return wi < 0 ? -wi : wi;
}

bool passCaveWeirdness(const BiomeNoise *bn, int bx, int bz, uint32_t sample_flags)
{
    int wi = quantWeirdness(bn, bx, bz, sample_flags);
    return wi < 500 || wi > 12800;
}

bool passCaveClimate(const BiomeNoise *bn, int bx, int bz, uint32_t sample_flags)
{
    if (!passCaveWeirdness(bn, bx, bz, sample_flags))
        return false;

    double px = bx, pz = bz;
    if (!(sample_flags & SAMPLE_NO_SHIFT))
    {
        px += sampleDoublePerlin(&bn->climate[NP_SHIFT], bx, 0, bz) * 4.0;
        pz += sampleDoublePerlin(&bn->climate[NP_SHIFT], bz, bx, 0) * 4.0;
    }

    int ci = (int) (10000.0 * sampleDoublePerlin(&bn->climate[NP_CONTINENTALNESS], px, 0, pz));
    if (ci <= 5500)
        return false;

    int ei = (int) (10000.0 * sampleDoublePerlin(&bn->climate[NP_EROSION], px, 0, pz));
    return ei > -3750;
}

void samplePreciseCell(const Generator *g, int worldX, int worldZ, int *riverHits, int *caveHits)
{
    *riverHits = 0;
    *caveHits = 0;

    const int bx = worldX / 4;
    const int bz = worldZ / 4;
    if (!passCaveClimate(&g->bn, bx, bz, COARSE_SAMPLE_FLAGS))
        return;

    for (int i = 0; i < PRECISE_Y_COUNT; i++)
    {
        const int id = getBiomeAt(g, 1, worldX, PRECISE_YS[i], worldZ);
        *riverHits += (id == river);
        *caveHits += (id == dripstone_caves);
    }
}
