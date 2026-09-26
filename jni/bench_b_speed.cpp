/**
 * Speed bench: live Cont/Ridge B (samplePerlin) vs noise-space bilinear tables.
 * Lattices: Cont@32 / Weird@16. A octaves stay live (unchanged).
 *
 * Build (from jni/):
 *   g++ ... OctaveFieldCache.cpp bench_b_speed.cpp + cubiomes objs
 */
#include "OctaveFieldCache.h"
#include "cubiomes/generator.h"
#include "cubiomes/biomes.h"
#include "cubiomes/noise.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

static constexpr double kF = OctaveBNoiseCache::kDoublePerlinF;

using Clock = std::chrono::steady_clock;

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

static bool contGate(
    const DoublePerlinNoise *cont,
    const OctaveACache *acache,
    const OctaveBNoiseCache *bcache,
    bool useCacheA,
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
        if (useCacheA && acache && acache->ready)
            return (double) acache->contA[oi].atClimate(bx, bz);
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

static bool weirdGate(
    const DoublePerlinNoise *ridge,
    const OctaveACache *acache,
    const OctaveBNoiseCache *bcache,
    bool useCacheA,
    bool useInterpB,
    int bx, int bz)
{
    double v = 0;
    const int octCnt = std::min(ridge->octA.octcnt, ridge->octB.octcnt);
    for (int oi = 0; oi < octCnt; oi++)
    {
        if (useCacheA && acache && acache->ready && oi < OctaveACache::RIDGE_OCT)
            v += (double) acache->ridgeA[oi].atClimate(bx, bz);
        else
            v += liveOctA(ridge->octA.octaves + oi, (double) bx, (double) bz);
        if (useInterpB && oi < OctaveBNoiseCache::RIDGE_OCT)
            v += bcache->ridgeB[oi].sampleClimate((double) bx, (double) bz);
        else
            v += liveOctB(ridge->octB.octaves + oi, (double) bx, (double) bz);
    }
    v *= ridge->amplitude;
    int wi = (int) (10000.0 * v);
    if (wi < 0) wi = -wi;
    return wi < 500 || wi > 12800;
}

struct Timed {
    double ms = 0;
    long long n = 0;
    long long pass = 0;
    uint64_t sink = 0;
};

template <typename Fn>
static Timed timeLoop(int repeats, Fn &&fn)
{
    Timed t;
    /* warmup */
    fn(t);
    t = {};
    const auto t0 = Clock::now();
    for (int r = 0; r < repeats; r++)
        fn(t);
    const auto t1 = Clock::now();
    t.ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return t;
}

static void printRate(const char *label, const Timed &t)
{
    const double sec = t.ms / 1000.0;
    const double mps = sec > 0 ? (double) t.n / sec / 1e6 : 0;
    std::printf("  %-36s  %8.1f ms  n=%lld  pass=%lld  %.2f Mpts/s  sink=%llu\n",
                label, t.ms, (long long) t.n, (long long) t.pass, mps,
                (unsigned long long) t.sink);
}

int main(int argc, char **argv)
{
    int64_t seed = -8180004378910677489LL;
    int half = 16384;
    int tableN = kDefaultBNoiseTableN;
    int threads = (int) std::thread::hardware_concurrency();
    if (threads < 1) threads = 1;
    int repeats = 3;
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
        else if (!std::strcmp(argv[i], "--repeats") && i + 1 < argc)
            repeats = std::atoi(argv[++i]);
    }

    std::printf("=== B interp SPEED vs live samplePerlin ===\n");
    std::printf("seed=%lld half=%d tableN=%d threads=%d repeats=%d\n",
                (long long) seed, half, tableN, threads, repeats);
    std::printf("A cache Cont@32/Weird@16 + B noise table vs live B.\n");
    std::printf("Window climate bx,bz in [%d, %d)\n\n", -half / 4, half / 4);

    Generator g;
    setupGenerator(&g, mc, FORCE_OCEAN_VARIANTS);
    applySeed(&g, DIM_OVERWORLD, (uint64_t) seed);

    const auto tb0 = Clock::now();
    OctaveBNoiseCache bcache;
    if (!buildOctaveBNoiseCache(&bcache, &g.bn, tableN, threads))
    {
        std::fprintf(stderr, "buildOctaveBNoiseCache failed\n");
        return 1;
    }
    const auto tb1 = Clock::now();
    const double buildBMs = std::chrono::duration<double, std::milli>(tb1 - tb0).count();
    const double memBMiB = (8.0 * (double) tableN * (double) tableN * 4.0) / (1024.0 * 1024.0);

    std::atomic_int progCur{0}, progTot{0};
    std::atomic_bool noPause{false}, noStop{false};
    const auto ta0 = Clock::now();
    OctaveACache acache;
    if (!buildOctaveACache(&acache, &g.bn, 32, 16, threads, &progCur, &progTot, &noPause, &noStop))
    {
        std::fprintf(stderr, "buildOctaveACache failed\n");
        return 1;
    }
    const auto ta1 = Clock::now();
    const double buildAMs = std::chrono::duration<double, std::milli>(ta1 - ta0).count();

    std::printf("Phase0 A tables Cont@32+Weird@16: build=%.1f ms\n", buildAMs);
    std::printf("Phase0 B tables: N=%d  ~%.1f MiB  build=%.1f ms (threads=%d)\n\n",
                tableN, memBMiB, buildBMs, threads);

    const DoublePerlinNoise *cont = &g.bn.climate[NP_CONTINENTALNESS];
    const DoublePerlinNoise *ridge = &g.bn.climate[NP_WEIRDNESS];

    const int bx0 = -half / 4;
    const int bx1 = half / 4;
    const int bz0 = bx0;
    const int bz1 = bx1;
    const int contStep = 8;
    const int contHalf = 4;
    const int weirdStep = 4;
    const int weirdHalf = 2;

    /* --- 1) Pure Cont B sum (all 5 octaves, no A, no early-exit) --- */
    auto pureContB = [&](bool useInterp, Timed &t) {
        for (int bz = bz0 + contHalf; bz < bz1; bz += contStep)
        {
            for (int bx = bx0 + contHalf; bx < bx1; bx += contStep)
            {
                double s = 0;
                for (int oi = 0; oi < OctaveBNoiseCache::CONT_OCT; oi++)
                {
                    if (useInterp)
                        s += bcache.contB[oi].sampleClimate((double) bx, (double) bz);
                    else
                        s += liveOctB(cont->octB.octaves + oi, (double) bx, (double) bz);
                }
                t.n++;
                t.sink += (uint64_t) (int64_t) (s * 1e6);
            }
        }
    };

    Timed livePureCont = timeLoop(repeats, [&](Timed &t) { pureContB(false, t); });
    Timed interpPureCont = timeLoop(repeats, [&](Timed &t) { pureContB(true, t); });

    std::printf("--- Pure Cont B (5 octaves / Cont@32 point) ---\n");
    printRate("live samplePerlin", livePureCont);
    printRate("interp table", interpPureCont);
    std::printf("  speedup: %.2fx\n\n", livePureCont.ms / interpPureCont.ms);

    /* --- 2) Pure Ridge B sum (3 octaves) @ Weird@16 --- */
    auto pureRidgeB = [&](bool useInterp, Timed &t) {
        for (int bz = bz0 + weirdHalf; bz < bz1; bz += weirdStep)
        {
            for (int bx = bx0 + weirdHalf; bx < bx1; bx += weirdStep)
            {
                double s = 0;
                for (int oi = 0; oi < OctaveBNoiseCache::RIDGE_OCT; oi++)
                {
                    if (useInterp)
                        s += bcache.ridgeB[oi].sampleClimate((double) bx, (double) bz);
                    else
                        s += liveOctB(ridge->octB.octaves + oi, (double) bx, (double) bz);
                }
                t.n++;
                t.sink += (uint64_t) (int64_t) (s * 1e6);
            }
        }
    };

    Timed livePureRidge = timeLoop(repeats, [&](Timed &t) { pureRidgeB(false, t); });
    Timed interpPureRidge = timeLoop(repeats, [&](Timed &t) { pureRidgeB(true, t); });

    std::printf("--- Pure Ridge B (3 octaves / Weird@16 point) ---\n");
    printRate("live samplePerlin", livePureRidge);
    printRate("interp table", interpPureRidge);
    std::printf("  speedup: %.2fx\n\n", livePureRidge.ms / interpPureRidge.ms);

    /* --- 3) Cont gate variants --- */
    auto runContGate = [&](bool useA, bool useInterpB, Timed &t) {
        for (int bz = bz0 + contHalf; bz < bz1; bz += contStep)
        {
            for (int bx = bx0 + contHalf; bx < bx1; bx += contStep)
            {
                const bool ok = contGate(cont, &acache, &bcache, useA, useInterpB, bx, bz, 0.525);
                t.n++;
                if (ok) t.pass++;
                t.sink += ok ? 1u : 0u;
            }
        }
    };

    Timed liveA_liveB_cont = timeLoop(repeats, [&](Timed &t) { runContGate(false, false, t); });
    Timed liveA_interpB_cont = timeLoop(repeats, [&](Timed &t) { runContGate(false, true, t); });
    Timed cacheA_liveB_cont = timeLoop(repeats, [&](Timed &t) { runContGate(true, false, t); });
    Timed cacheA_interpB_cont = timeLoop(repeats, [&](Timed &t) { runContGate(true, true, t); });

    std::printf("--- Cont gate Cont@32 ---\n");
    printRate("A live  + B live", liveA_liveB_cont);
    printRate("A live  + B interp", liveA_interpB_cont);
    printRate("A cache + B live  (status quo)", cacheA_liveB_cont);
    printRate("A cache + B interp (budget B)", cacheA_interpB_cont);
    std::printf("  B-only delta (A live):   %.2fx\n", liveA_liveB_cont.ms / liveA_interpB_cont.ms);
    std::printf("  B-only delta (A cache):  %.2fx  << product-relevant\n\n",
                cacheA_liveB_cont.ms / cacheA_interpB_cont.ms);

    /* --- 4) Weird gate variants --- */
    auto runWeirdGate = [&](bool useA, bool useInterpB, Timed &t) {
        for (int bz = bz0 + weirdHalf; bz < bz1; bz += weirdStep)
        {
            for (int bx = bx0 + weirdHalf; bx < bx1; bx += weirdStep)
            {
                const bool ok = weirdGate(ridge, &acache, &bcache, useA, useInterpB, bx, bz);
                t.n++;
                if (ok) t.pass++;
                t.sink += ok ? 1u : 0u;
            }
        }
    };

    Timed liveA_liveB_weird = timeLoop(repeats, [&](Timed &t) { runWeirdGate(false, false, t); });
    Timed liveA_interpB_weird = timeLoop(repeats, [&](Timed &t) { runWeirdGate(false, true, t); });
    Timed cacheA_liveB_weird = timeLoop(repeats, [&](Timed &t) { runWeirdGate(true, false, t); });
    Timed cacheA_interpB_weird = timeLoop(repeats, [&](Timed &t) { runWeirdGate(true, true, t); });

    std::printf("--- Weird gate Weird@16 ---\n");
    printRate("A live  + B live", liveA_liveB_weird);
    printRate("A live  + B interp", liveA_interpB_weird);
    printRate("A cache + B live  (status quo)", cacheA_liveB_weird);
    printRate("A cache + B interp (budget B)", cacheA_interpB_weird);
    std::printf("  B-only delta (A live):   %.2fx\n", liveA_liveB_weird.ms / liveA_interpB_weird.ms);
    std::printf("  B-only delta (A cache):  %.2fx  << product-relevant\n\n",
                cacheA_liveB_weird.ms / cacheA_interpB_weird.ms);

    /* --- 5) Product-like Cont@32 → Weird@16 --- */
    auto runProduct = [&](bool useA, bool useInterpB, Timed &t) {
        for (int bz = bz0 + contHalf; bz < bz1; bz += contStep)
        {
            for (int bx = bx0 + contHalf; bx < bx1; bx += contStep)
            {
                t.n++;
                if (!contGate(cont, &acache, &bcache, useA, useInterpB, bx, bz, 0.525))
                    continue;
                bool any = false;
                for (int dz = -2; dz <= 2; dz += 4)
                {
                    for (int dx = -2; dx <= 2; dx += 4)
                    {
                        const int wx = bx + dx;
                        const int wz = bz + dz;
                        if (wx < bx0 || wx >= bx1 || wz < bz0 || wz >= bz1)
                            continue;
                        if (weirdGate(ridge, &acache, &bcache, useA, useInterpB, wx, wz))
                            any = true;
                    }
                }
                if (any) t.pass++;
                t.sink += any ? 1u : 0u;
            }
        }
    };

    Timed liveA_liveB_prod = timeLoop(repeats, [&](Timed &t) { runProduct(false, false, t); });
    Timed liveA_interpB_prod = timeLoop(repeats, [&](Timed &t) { runProduct(false, true, t); });
    Timed cacheA_liveB_prod = timeLoop(repeats, [&](Timed &t) { runProduct(true, false, t); });
    Timed cacheA_interpB_prod = timeLoop(repeats, [&](Timed &t) { runProduct(true, true, t); });

    std::printf("--- Cont@32 then Weird@16 (product-like) ---\n");
    printRate("A live  + B live", liveA_liveB_prod);
    printRate("A live  + B interp", liveA_interpB_prod);
    printRate("A cache + B live  (status quo)", cacheA_liveB_prod);
    printRate("A cache + B interp (budget B)", cacheA_interpB_prod);
    std::printf("  B-only delta (A live):   %.2fx\n", liveA_liveB_prod.ms / liveA_interpB_prod.ms);
    std::printf("  B-only delta (A cache):  %.2fx  << product-relevant\n\n",
                cacheA_liveB_prod.ms / cacheA_interpB_prod.ms);

    /* Summary */
    std::printf("=== Summary ===\n");
    std::printf("Pure Cont-B only (no A):           %.2fx\n", livePureCont.ms / interpPureCont.ms);
    std::printf("Pure Ridge-B only (no A):          %.2fx\n", livePureRidge.ms / interpPureRidge.ms);
    std::printf("Cont gate  | B vs live | A cached: %.2fx\n",
                cacheA_liveB_cont.ms / cacheA_interpB_cont.ms);
    std::printf("Weird gate | B vs live | A cached: %.2fx\n",
                cacheA_liveB_weird.ms / cacheA_interpB_weird.ms);
    std::printf("Product    | B vs live | A cached: %.2fx\n",
                cacheA_liveB_prod.ms / cacheA_interpB_prod.ms);
    std::printf("A-cache alone vs all-live product: %.2fx\n",
                liveA_liveB_prod.ms / cacheA_liveB_prod.ms);
    std::printf("A+B budget vs all-live product:    %.2fx\n",
                liveA_liveB_prod.ms / cacheA_interpB_prod.ms);
    std::printf("B table build once: %.1f ms (~%.1f MiB)\n", buildBMs, memBMiB);

    return 0;
}
