#include "BiomeSampler.h"
#include "cubiomes/biomes.h"
#include "cubiomes/noise.h"
#include <cstdlib>

const int PRECISE_YS[PRECISE_Y_COUNT] = { -60, -56, -52 };

static constexpr double kDoublePerlinF = 337.0 / 331.0;

static double sampleContOctA(const DoublePerlinNoise *dpn, int idx, double x, double y, double z)
{
    if (idx < 0 || idx >= dpn->octA.octcnt)
        return 0.0;
    const PerlinNoise *p = dpn->octA.octaves + idx;
    const double lf = p->lacunarity;
    const double ax = maintainPrecision(x * lf);
    const double ay = maintainPrecision(y * lf);
    const double az = maintainPrecision(z * lf);
    return p->amplitude * samplePerlin(p, ax, ay, az, 0, 0);
}

static double sampleContOctB(const DoublePerlinNoise *dpn, int idx, double x, double y, double z)
{
    if (idx < 0 || idx >= dpn->octB.octcnt)
        return 0.0;
    const PerlinNoise *p = dpn->octB.octaves + idx;
    const double lf = p->lacunarity;
    const double ax = maintainPrecision(x * lf * kDoublePerlinF);
    const double ay = maintainPrecision(y * lf * kDoublePerlinF);
    const double az = maintainPrecision(z * lf * kDoublePerlinF);
    return p->amplitude * samplePerlin(p, ax, ay, az, 0, 0);
}

bool passContinentalnessPartialThr(const BiomeNoise *bn, int bx, int bz, double threshold)
{
    const DoublePerlinNoise *dpn = &bn->climate[NP_CONTINENTALNESS];
    // sampleDoublePerlin scales octA+octB by this; keep running sum in climate units
    const double amp = dpn->amplitude;
    const double x = bx;
    const double y = 0.0;
    const double z = bz;
    double sum = 0.0;

    sum += amp * (sampleContOctA(dpn, 0, x, y, z) + sampleContOctB(dpn, 0, x, y, z));
    if (sum < -0.2)
        return false;

    sum += amp * sampleContOctA(dpn, 1, x, y, z);
    if (sum < -0.1)
        return false;

    sum += amp * sampleContOctB(dpn, 1, x, y, z);
    if (sum < 0.0)
        return false;

    sum += amp * sampleContOctA(dpn, 2, x, y, z);
    if (sum < 0.13)
        return false;

    sum += amp * sampleContOctB(dpn, 2, x, y, z);
    if (sum < 0.3)
        return false;

    sum += amp * sampleContOctA(dpn, 3, x, y, z);
    if (sum < 0.37)
        return false;

    sum += amp * sampleContOctB(dpn, 3, x, y, z);
    if (sum < 0.44)
        return false;

    sum += amp * (sampleContOctA(dpn, 4, x, y, z) + sampleContOctB(dpn, 4, x, y, z));
    return sum > threshold;
}

bool passContinentalnessPartial(const BiomeNoise *bn, int bx, int bz)
{
    return passContinentalnessPartialThr(bn, bx, bz, CONT_PARTIAL_FINAL_THRESHOLD);
}

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

bool passCoarseCaveCell(const BiomeNoise *bn, int bx, int bz, uint32_t sample_flags)
{
    if (!passContinentalnessPartial(bn, bx, bz))
        return false;
    if (!passCaveWeirdness(bn, bx, bz, sample_flags))
        return false;

    double px = bx, pz = bz;
    if (!(sample_flags & SAMPLE_NO_SHIFT))
    {
        px += sampleDoublePerlin(&bn->climate[NP_SHIFT], bx, 0, bz) * 4.0;
        pz += sampleDoublePerlin(&bn->climate[NP_SHIFT], bz, bx, 0) * 4.0;
    }

    int ei = (int) (10000.0 * sampleDoublePerlin(&bn->climate[NP_EROSION], px, 0, pz));
    return ei > -3750;
}

bool passCaveClimate(const BiomeNoise *bn, int bx, int bz, uint32_t sample_flags)
{
    return passCoarseCaveCell(bn, bx, bz, sample_flags);
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
