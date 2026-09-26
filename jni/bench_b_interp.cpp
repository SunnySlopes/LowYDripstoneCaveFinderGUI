/**
 * Accuracy bench: interpolated Cont/Ridge B (noise-space table) vs live samplePerlin
 * on Cont@32 and Weird@16 climate lattices.
 */
#include "OctaveFieldCache.h"
#include "cubiomes/generator.h"
#include "cubiomes/biomes.h"
#include "cubiomes/noise.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <thread>

static constexpr double kF = OctaveBNoiseCache::kDoublePerlinF;

struct ErrStats {
    long long n = 0;
    double sumAbs = 0;
    double sumSq = 0;
    double maxAbs = 0;
    void add(double e)
    {
        const double a = std::fabs(e);
        n++;
        sumAbs += a;
        sumSq += e * e;
        if (a > maxAbs)
            maxAbs = a;
    }
    double mae() const { return n ? sumAbs / (double) n : 0; }
    double rmse() const { return n ? std::sqrt(sumSq / (double) n) : 0; }
};

static double liveOctA(const PerlinNoise *p, double bx, double bz)
{
    const double lf = p->lacunarity;
    return p->amplitude * samplePerlin(p,
        maintainPrecision(bx * lf), 0.0, maintainPrecision(bz * lf), 0, 0);
}

static double liveOctB(const PerlinNoise *p, double bx, double bz)
{
    const double lf = p->lacunarity;
    return p->amplitude * samplePerlin(p,
        maintainPrecision(bx * lf * kF), 0.0, maintainPrecision(bz * lf * kF), 0, 0);
}

static void printOctStats(const char *name, const ErrStats &st)
{
    std::printf("  %-10s  N=%lld  MAE=%.6e  RMSE=%.6e  maxAbs=%.6e\n",
                name, (long long) st.n, st.mae(), st.rmse(), st.maxAbs);
}

/** Cont early-exit final accept (thr), using chosen B sampler. Returns whether accepted. */
static bool contGateAccept(
    const DoublePerlinNoise *cont,
    const OctaveBNoiseCache *bcache,
    bool useInterpB,
    int bx, int bz,
    double thr)
{
    const double amp = cont->amplitude;
    const double x = bx, z = bz;
    auto B = [&](int oi) -> double {
        if (useInterpB)
            return bcache->contB[oi].sampleClimate(x, z);
        return liveOctB(cont->octB.octaves + oi, x, z);
    };
    auto A = [&](int oi) -> double {
        return liveOctA(cont->octA.octaves + oi, x, z);
    };

    double sum = amp * (A(0) + B(0));
    if (sum < -0.2) return false;
    sum += amp * A(1);
    if (sum < -0.1) return false;
    sum += amp * B(1);
    if (sum < 0.0) return false;
    sum += amp * A(2);
    if (sum < 0.13) return false;
    sum += amp * B(2);
    if (sum < 0.3) return false;
    sum += amp * A(3);
    if (sum < 0.37) return false;
    sum += amp * B(3);
    if (sum < 0.44) return false;
    sum += amp * (A(4) + B(4));
    return sum > thr;
}

int main(int argc, char **argv)
{
    int64_t seed = -8180004378910677489LL;
    int half = 16384; /* world half → climate ±half/4 */
    int tableN = kDefaultBNoiseTableN;
    int threads = (int) std::thread::hardware_concurrency();
    if (threads < 1) threads = 1;
    int mc = MC_26_1;

    for (int i = 1; i < argc; i++)
    {
        if (!std::strcmp(argv[i], "--seed") && i + 1 < argc)
            seed = std::strtoll(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--half") && i + 1 < argc)
            half = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--n") && i + 1 < argc)
            tableN = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--threads") && i + 1 < argc)
            threads = std::atoi(argv[++i]);
    }

    std::printf("=== B noise-table interp vs live samplePerlin ===\n");
    std::printf("seed=%lld half=%d tableN=%d threads=%d\n",
                (long long) seed, half, tableN, threads);
    std::printf("Query lattices: Cont@32 (climate step 8), Weird@16 (climate step 4)\n");
    std::printf("Window climate bx,bz in [%d, %d)\n\n", -half / 4, half / 4);

    Generator g;
    setupGenerator(&g, mc, FORCE_OCEAN_VARIANTS);
    applySeed(&g, DIM_OVERWORLD, (uint64_t) seed);

    OctaveBNoiseCache bcache;
    if (!buildOctaveBNoiseCache(&bcache, &g.bn, tableN, threads))
    {
        std::fprintf(stderr, "buildOctaveBNoiseCache failed (octB.octcnt?)\n");
        return 1;
    }
    const double memMiB = (8.0 * (double) tableN * (double) tableN * 4.0) / (1024.0 * 1024.0);
    std::printf("Built 8 B tables: N=%d  ~%.1f MiB\n\n", tableN, memMiB);

    const DoublePerlinNoise *cont = &g.bn.climate[NP_CONTINENTALNESS];
    const DoublePerlinNoise *ridge = &g.bn.climate[NP_WEIRDNESS];

    const int bx0 = -half / 4;
    const int bx1 = half / 4;
    const int bz0 = bx0;
    const int bz1 = bx1;

    ErrStats contOct[OctaveBNoiseCache::CONT_OCT];
    long long contGateDisagree = 0;
    long long contGateSamples = 0;
    const int contStepClim = 8;
    const int contHalf = 4;

    for (int bz = bz0 + contHalf; bz < bz1; bz += contStepClim)
    {
        for (int bx = bx0 + contHalf; bx < bx1; bx += contStepClim)
        {
            for (int oi = 0; oi < OctaveBNoiseCache::CONT_OCT; oi++)
            {
                const double live = liveOctB(cont->octB.octaves + oi, (double) bx, (double) bz);
                const double interp = bcache.contB[oi].sampleClimate((double) bx, (double) bz);
                contOct[oi].add(interp - live);
            }
            const bool livePass = contGateAccept(cont, &bcache, false, bx, bz, 0.525);
            const bool interpPass = contGateAccept(cont, &bcache, true, bx, bz, 0.525);
            contGateSamples++;
            if (livePass != interpPass)
                contGateDisagree++;
        }
    }

    std::printf("--- Cont B octaves @ Cont@32 lattice ---\n");
    for (int i = 0; i < OctaveBNoiseCache::CONT_OCT; i++)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Cont %dB", i);
        printOctStats(buf, contOct[i]);
    }
    std::printf("  Cont gate thr=0.525: samples=%lld disagree=%lld (%.6f%%)\n\n",
                (long long) contGateSamples, (long long) contGateDisagree,
                contGateSamples ? 100.0 * (double) contGateDisagree / (double) contGateSamples : 0.0);

    ErrStats ridgeOct[OctaveBNoiseCache::RIDGE_OCT];
    long long weirdGateDisagree = 0;
    long long weirdGateSamples = 0;
    const int weirdStepClim = 4;
    const int weirdHalf = 2;

    for (int bz = bz0 + weirdHalf; bz < bz1; bz += weirdStepClim)
    {
        for (int bx = bx0 + weirdHalf; bx < bx1; bx += weirdStepClim)
        {
            double vLive = 0;
            double vInterp = 0;
            const int octCnt = std::min(ridge->octA.octcnt, ridge->octB.octcnt);
            for (int oi = 0; oi < octCnt; oi++)
            {
                const double a = liveOctA(ridge->octA.octaves + oi, (double) bx, (double) bz);
                const double bLive = liveOctB(ridge->octB.octaves + oi, (double) bx, (double) bz);
                const double bInterp = (oi < OctaveBNoiseCache::RIDGE_OCT)
                    ? bcache.ridgeB[oi].sampleClimate((double) bx, (double) bz)
                    : bLive;
                if (oi < OctaveBNoiseCache::RIDGE_OCT)
                    ridgeOct[oi].add(bInterp - bLive);
                vLive += a + bLive;
                vInterp += a + bInterp;
            }
            vLive *= ridge->amplitude;
            vInterp *= ridge->amplitude;
            int wiL = (int) (10000.0 * vLive);
            if (wiL < 0) wiL = -wiL;
            int wiI = (int) (10000.0 * vInterp);
            if (wiI < 0) wiI = -wiI;
            const bool passL = wiL < 500 || wiL > 12800;
            const bool passI = wiI < 500 || wiI > 12800;
            weirdGateSamples++;
            if (passL != passI)
                weirdGateDisagree++;
        }
    }

    std::printf("--- Ridge B octaves @ Weird@16 lattice ---\n");
    for (int i = 0; i < OctaveBNoiseCache::RIDGE_OCT; i++)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Ridge %dB", i);
        printOctStats(buf, ridgeOct[i]);
    }
    std::printf("  Weird gate: samples=%lld disagree=%lld (%.6f%%)\n",
                (long long) weirdGateSamples, (long long) weirdGateDisagree,
                weirdGateSamples ? 100.0 * (double) weirdGateDisagree / (double) weirdGateSamples : 0.0);

    return 0;
}
