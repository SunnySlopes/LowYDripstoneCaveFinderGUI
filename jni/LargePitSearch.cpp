#include "cubiomes/biomenoise.h"
#include "cubiomes/noise.h"
#include "SearchConfig.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ranges>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * Full-map large-pit search.
 * Octave contributions are in sampleDoublePerlin units: each Perlin term is
 * multiplied by DoublePerlinNoise::amplitude, so 0A+1A+2A+0B+1B+2B == full W.
 * A octaves are finished inside one 131072-block period before any B sample.
 */

static constexpr double kDpnF = 337.0 / 331.0;
static constexpr int kStep1Min = -65536;
static constexpr int kStep1Max = 65536;
static constexpr int kStep1Stride = 128;
static constexpr int kStep1N = (kStep1Max - kStep1Min) / kStep1Stride;
static constexpr double kA0Min = 0.21;
static constexpr int kStep2Radius = 128;
static constexpr int kStep2Stride = 64;
static constexpr double kA01Min = 0.55;
static constexpr int kStep3Radius = 64;
static constexpr int kStep3Stride = 16;
static constexpr double kA012Min = 0.6;
static constexpr int kStep3Dedup = 192;
static constexpr int kPeriod = 131072;
static constexpr double kB0Min = 1.0;
static constexpr double kB1Min = 1.4;
static constexpr int kWCoarseRadius = 128;
static constexpr int kWCoarseStride = 64;
static constexpr int kWFineRadius = 40;
static constexpr int kWFineStride = 4;
static constexpr double kWMinExclusive = 1.6;
static constexpr int kWorldLimit = 30000000;
/** Cover the world border. B is not periodic, so every replica inside the border is sampled. */
static constexpr int kPeriodKMax = (kWorldLimit + 65536 + kPeriod - 1) / kPeriod;
static constexpr int kPeakDedup = 256;
static constexpr double kContMin = 0.3;
/** Ring centers are searched inside this Chebyshev radius of the W peak. */
static constexpr int kCenterRadius = 128;
/** Window must also cover the outer ring around those centers. */
static constexpr int kAreaOrigin = kCenterRadius + 128;
static constexpr int kAreaWindow = kAreaOrigin * 2;
static constexpr int kMaxSeeds = kStep1N * kStep1N;

struct PitSeed {
    int bx;
    int bz;
    int sign;
};

struct PitExtreme {
    int bx;
    int bz;
    double a012;
    int sign;
};

struct PitPeak {
    int bx;
    int bz;
    double w;
};

static bool pitPausedOrStopped(Progress *progress)
{
    if (!progress)
        return false;
    while (progress->try_pause.load())
    {
        if (progress->try_stop.load())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return progress->try_stop.load();
}

static double rawOctave(const PerlinNoise *p, double x, double z)
{
    const double lf = p->lacunarity;
    const double ax = maintainPrecision(x * lf);
    const double ay = maintainPrecision(0.0 * lf);
    const double az = maintainPrecision(z * lf);
    return p->amplitude * samplePerlin(p, ax, ay, az, 0, 0);
}

/** One octave in the same units as sampleDoublePerlin. */
static double scaledOctave(const DoublePerlinNoise *dpn, bool bSide, int idx, double climateX, double climateZ)
{
    const OctaveNoise *oct = bSide ? &dpn->octB : &dpn->octA;
    if (idx < 0 || idx >= oct->octcnt)
        return 0.0;
    const double x = bSide ? climateX * kDpnF : climateX;
    const double z = bSide ? climateZ * kDpnF : climateZ;
    return dpn->amplitude * rawOctave(oct->octaves + idx, x, z);
}

static double fullWeirdness(const DoublePerlinNoise *dpn, int blockX, int blockZ)
{
    const double x = blockX * 0.25;
    const double z = blockZ * 0.25;
    double sum = 0.0;
    const int n = std::max(dpn->octA.octcnt, dpn->octB.octcnt);
    for (int i = 0; i < n; i++)
    {
        sum += scaledOctave(dpn, false, i, x, z);
        sum += scaledOctave(dpn, true, i, x, z);
    }
    return sum;
}

static void verifyWeirdnessUnits(const DoublePerlinNoise *dpn)
{
    const double sum = fullWeirdness(dpn, 0, 0);
    const double full = sampleDoublePerlin(dpn, 0.0, 0.0, 0.0);
    if (std::fabs(sum - full) > 1e-6)
    {
        std::fprintf(stderr,
                     "large-pit weirdness unit mismatch: six-octave sum=%.9f sampleDoublePerlin=%.9f\n",
                     sum, full);
    }
}

static double contTenOctaves(const DoublePerlinNoise *dpn, int blockX, int blockZ)
{
    const double x = blockX * 0.25;
    const double z = blockZ * 0.25;
    double sum = 0.0;
    for (int i = 0; i < 5; i++)
    {
        sum += scaledOctave(dpn, false, i, x, z);
        sum += scaledOctave(dpn, true, i, x, z);
    }
    return sum;
}

static bool insideWorld(int x, int z)
{
    return x >= -kWorldLimit && x <= kWorldLimit && z >= -kWorldLimit && z <= kWorldLimit;
}

static bool chebyshevWithin(int ax, int az, int bx, int bz, int limit)
{
    const int dx = std::abs(ax - bx);
    const int dz = std::abs(az - bz);
    return dx <= limit && dz <= limit;
}

static double aSum(const DoublePerlinNoise *dpn, int blockX, int blockZ, int lastIdx)
{
    const double x = blockX * 0.25;
    const double z = blockZ * 0.25;
    double sum = 0.0;
    for (int i = 0; i <= lastIdx; i++)
        sum += scaledOctave(dpn, false, i, x, z);
    return sum;
}

static std::vector<PitSeed> scanA0(const DoublePerlinNoise *dpn, Progress *progress, int threads)
{
    std::vector<uint8_t> hits((size_t) kStep1N * kStep1N, 0);
    if (progress)
    {
        progress->phase1.store(1);
        progress->current.store(0);
        progress->total.store(kStep1N);
        progress->chunkInRunning.store(0);
    }

    std::atomic<int> rowsDone{0};
    {
        ThreadPool pool((size_t) std::max(1, threads));
        const int band = std::max(1, kStep1N / (std::max(1, threads) * 4));
        for (int r0 = 0; r0 < kStep1N; r0 += band)
        {
            const int r1 = std::min(kStep1N, r0 + band);
            pool.enqueue([&, r0, r1]() {
                if (pitPausedOrStopped(progress))
                    return;
                if (progress)
                    progress->chunkInRunning.fetch_add(1);
                for (int ix = r0; ix < r1; ix++)
                {
                    if (progress && progress->try_stop.load())
                        break;
                    const int bx = kStep1Min + ix * kStep1Stride;
                    for (int iz = 0; iz < kStep1N; iz++)
                    {
                        const int bz = kStep1Min + iz * kStep1Stride;
                        const double o0 = scaledOctave(dpn, false, 0, bx * 0.25, bz * 0.25);
                        /* |0A| > 0.21. 1 = positive lobe, 2 = negative lobe. */
                        if (o0 > kA0Min)
                            hits[(size_t) ix * kStep1N + (size_t) iz] = 1;
                        else if (o0 < -kA0Min)
                            hits[(size_t) ix * kStep1N + (size_t) iz] = 2;
                    }
                }
                const int done = rowsDone.fetch_add(r1 - r0) + (r1 - r0);
                if (progress)
                {
                    progress->current.store(std::min(done, kStep1N));
                    progress->chunkInRunning.fetch_sub(1);
                }
            });
        }
    }

    std::vector<PitSeed> seeds;
    seeds.reserve(4096);
    for (int ix = 0; ix < kStep1N; ix++)
    {
        for (int iz = 0; iz < kStep1N; iz++)
        {
            const uint8_t mark = hits[(size_t) ix * kStep1N + (size_t) iz];
            if (!mark)
                continue;
            if ((int) seeds.size() >= kMaxSeeds)
                return seeds;
            seeds.push_back(PitSeed{kStep1Min + ix * kStep1Stride, kStep1Min + iz * kStep1Stride,
                                    mark == 1 ? 1 : -1});
        }
    }
    return seeds;
}

static void beginPhase(Progress *progress, int phase, int total)
{
    if (!progress)
        return;
    progress->current.store(0);
    progress->total.store(std::max(1, total));
    progress->chunkInRunning.store(0);
    progress->phase1.store(phase);
}

static std::vector<PitExtreme> refineAChain(const DoublePerlinNoise *dpn, const std::vector<PitSeed> &seeds,
                                            Progress *progress)
{
    beginPhase(progress, 1, (int) seeds.size());
    std::vector<PitExtreme> scratch;
    scratch.reserve(seeds.size() / 8 + 1);
    std::unordered_set<uint64_t> seen;
    seen.reserve(seeds.size() / 8 + 1);

    int index = 0;
    for (const PitSeed &seed : seeds)
    {
        if (pitPausedOrStopped(progress))
            return {};
        if (progress)
            progress->current.store(index);
        index++;

        const int sign = seed.sign;
        int bestX = seed.bx;
        int bestZ = seed.bz;
        double best = sign > 0 ? -1e300 : 1e300;
        auto moreExtreme = [sign](double s, double bestSoFar) {
            return sign > 0 ? s > bestSoFar : s < bestSoFar;
        };
        for (int rx = seed.bx - kStep2Radius; rx <= seed.bx + kStep2Radius; rx += kStep2Stride)
        {
            for (int rz = seed.bz - kStep2Radius; rz <= seed.bz + kStep2Radius; rz += kStep2Stride)
            {
                const double s = aSum(dpn, rx, rz, 1);
                if (moreExtreme(s, best))
                {
                    best = s;
                    bestX = rx;
                    bestZ = rz;
                }
            }
        }
        if (sign > 0 ? !(best > kA01Min) : !(best < -kA01Min))
            continue;

        const uint64_t key2 = ((uint64_t) (uint32_t) bestX << 32) | (uint32_t) bestZ | (sign < 0 ? 0x8000000000000000ULL : 0);
        if (seen.contains(key2))
            continue;

        best = sign > 0 ? -1e300 : 1e300;
        double a012 = 0.0;
        for (int rx = bestX - kStep3Radius; rx <= bestX + kStep3Radius; rx += kStep3Stride)
        {
            for (int rz = bestZ - kStep3Radius; rz <= bestZ + kStep3Radius; rz += kStep3Stride)
            {
                const double s = aSum(dpn, rx, rz, 2);
                if (moreExtreme(s, best))
                {
                    best = s;
                    bestX = rx;
                    bestZ = rz;
                    a012 = s;
                }
            }
        }
        if (sign > 0 ? !(a012 > kA012Min) : !(a012 < -kA012Min))
            continue;
        const uint64_t key3 = ((uint64_t) (uint32_t) bestX << 32) | (uint32_t) bestZ | (sign < 0 ? 0x8000000000000000ULL : 0);
        if (!seen.insert(key3).second)
            continue;
        scratch.push_back(PitExtreme{bestX, bestZ, a012, sign});
    }

    std::ranges::sort(scratch, [](const PitExtreme &a, const PitExtreme &b) {
        return std::fabs(a.a012) > std::fabs(b.a012);
    });

    std::vector<PitExtreme> kept;
    kept.reserve(scratch.size());
    for (const PitExtreme &e : scratch)
    {
        bool dominated = false;
        for (const PitExtreme &k : kept)
        {
            if (e.sign == k.sign && chebyshevWithin(e.bx, e.bz, k.bx, k.bz, kStep3Dedup))
            {
                dominated = true;
                break;
            }
        }
        if (dominated)
            continue;
        kept.push_back(e);
    }
    if (progress)
        progress->current.store(index);
    return kept;
}

/** Keep the most extreme signed W. |W| > 1.6 stays, including |W| > 1.9. */
static bool refineSignedW(const DoublePerlinNoise *dpn, int cx, int cz, int sign, int &outX, int &outZ, double &outW)
{
    auto windowExt = [&](int centerX, int centerZ, int radius, int stride, int &bx, int &bz, double &best) {
        best = sign > 0 ? -1e300 : 1e300;
        bx = centerX;
        bz = centerZ;
        for (int rx = centerX - radius; rx <= centerX + radius; rx += stride)
        {
            for (int rz = centerZ - radius; rz <= centerZ + radius; rz += stride)
            {
                const double w = fullWeirdness(dpn, rx, rz);
                if (sign > 0 ? w > best : w < best)
                {
                    best = w;
                    bx = rx;
                    bz = rz;
                }
            }
        }
    };

    int cx2 = cx;
    int cz2 = cz;
    double w1 = 0.0;
    windowExt(cx, cz, kWCoarseRadius, kWCoarseStride, cx2, cz2, w1);
    windowExt(cx2, cz2, kWFineRadius, kWFineStride, outX, outZ, outW);
    if (!(std::fabs(outW) > kWMinExclusive))
        return false;
    return insideWorld(outX, outZ);
}

static std::vector<PitPeak> dedupPeaks(std::vector<PitPeak> peaks)
{
    std::ranges::sort(peaks, [](const PitPeak &a, const PitPeak &b) {
        const double aw = std::fabs(a.w);
        const double bw = std::fabs(b.w);
        if (aw != bw)
            return aw > bw;
        if (a.bx != b.bx)
            return a.bx < b.bx;
        return a.bz < b.bz;
    });
    auto cellOf = [](int v) {
        return v >= 0 ? v / kPeakDedup : (v - kPeakDedup + 1) / kPeakDedup;
    };
    auto bucketKey = [](int cx, int cz) {
        return ((uint64_t) (uint32_t) cx << 32) | (uint32_t) cz;
    };
    std::unordered_map<uint64_t, std::vector<PitPeak>> buckets;
    std::vector<PitPeak> kept;
    kept.reserve(peaks.size());
    for (const PitPeak &p : peaks)
    {
        if (!(std::fabs(p.w) > kWMinExclusive))
            continue;
        const int cx = cellOf(p.bx);
        const int cz = cellOf(p.bz);
        bool near = false;
        for (int dx = -1; dx <= 1 && !near; dx++)
        {
            for (int dz = -1; dz <= 1 && !near; dz++)
            {
                const auto it = buckets.find(bucketKey(cx + dx, cz + dz));
                if (it == buckets.end())
                    continue;
                for (const PitPeak &k : it->second)
                {
                    if (chebyshevWithin(p.bx, p.bz, k.bx, k.bz, kPeakDedup))
                    {
                        near = true;
                        break;
                    }
                }
            }
        }
        if (near)
            continue;
        buckets[bucketKey(cx, cz)].push_back(p);
        kept.push_back(p);
    }
    return kept;
}

static std::vector<PitPeak> scanBChain(const DoublePerlinNoise *dpn, const std::vector<PitExtreme> &extremes,
                                       Progress *progress, int threads)
{
    if (progress)
    {
        progress->phase1.store(1);
        progress->current.store(0);
        progress->total.store(std::max(1, (int) extremes.size()));
        progress->chunkInRunning.store(0);
    }
    if (extremes.empty())
        return {};

    std::vector<std::vector<PitPeak>> perExtreme(extremes.size());
    {
        ThreadPool pool((size_t) std::max(1, threads));
        for (size_t ei = 0; ei < extremes.size(); ei++)
        {
            pool.enqueue([&, ei]() {
                if (pitPausedOrStopped(progress))
                    return;
                if (progress)
                    progress->chunkInRunning.fetch_add(1);
                const PitExtreme &extreme = extremes[ei];
                std::vector<PitPeak> local;
                for (int kx = -kPeriodKMax; kx <= kPeriodKMax; kx++)
                {
                    if (progress && progress->try_stop.load())
                        break;
                    for (int kz = -kPeriodKMax; kz <= kPeriodKMax; kz++)
                    {
                        const long long px64 = (long long) extreme.bx + (long long) kx * kPeriod;
                        const long long pz64 = (long long) extreme.bz + (long long) kz * kPeriod;
                        if (px64 < -kWorldLimit || px64 > kWorldLimit || pz64 < -kWorldLimit || pz64 > kWorldLimit)
                            continue;
                        const int px = (int) px64;
                        const int pz = (int) pz64;
                        const double a012 = aSum(dpn, px, pz, 2);
                        const double x = px * 0.25;
                        const double z = pz * 0.25;
                        const double b0 = scaledOctave(dpn, true, 0, x, z);
                        if (!(std::fabs(a012 + b0) > kB0Min))
                            continue;
                        const double b1 = scaledOctave(dpn, true, 1, x, z);
                        if (!(std::fabs(a012 + b0 + b1) > kB1Min))
                            continue;
                        int hx = px;
                        int hz = pz;
                        double hw = 0.0;
                        if (!refineSignedW(dpn, px, pz, extreme.sign, hx, hz, hw))
                            continue;
                        local.push_back(PitPeak{hx, hz, hw});
                    }
                }
                perExtreme[ei] = std::move(local);
                if (progress)
                {
                    progress->current.fetch_add(1);
                    progress->chunkInRunning.fetch_sub(1);
                }
            });
        }
    }

    std::vector<PitPeak> peaks;
    for (const auto &part : perExtreme)
    {
        peaks.insert(peaks.end(), part.begin(), part.end());
    }
    return dedupPeaks(std::move(peaks));
}

static Res bestOf(std::vector<Res> found)
{
    Res best;
    for (const Res &r : found)
    {
        if (r.area > best.area)
            best = r;
    }
    return best;
}

static std::vector<PitPeak> filterContinentalness(const DoublePerlinNoise *cont, const std::vector<PitPeak> &peaks,
                                                  Progress *progress)
{
    beginPhase(progress, 2, (int) peaks.size());
    std::vector<PitPeak> passed;
    passed.reserve(peaks.size());
    int index = 0;
    for (const PitPeak &p : peaks)
    {
        if (pitPausedOrStopped(progress))
            return {};
        if (contTenOctaves(cont, p.bx, p.bz) >= kContMin)
            passed.push_back(p);
        index++;
        if (progress)
            progress->current.store(index);
    }
    return passed;
}

static std::vector<Res> scoreBiomes(Generator *g, const std::vector<PitPeak> &peaks, int minArea,
                                    float riverWeight, int threads, Progress *progress)
{
    beginPhase(progress, 3, (int) peaks.size());
    if (peaks.empty())
        return {};

    const uint64_t seedVal = g->seed;
    const int mcVer = g->mc > 0 ? g->mc : MC_26_1;
    ThreadSafeResults<Res> found;
    {
        ThreadPool pool((size_t) std::max(1, threads));
        for (const PitPeak &peak : peaks)
        {
            pool.enqueue([&, peak, seedVal, mcVer, minArea, riverWeight]() {
                if (pitPausedOrStopped(progress))
                    return;
                if (progress)
                    progress->chunkInRunning.fetch_add(1);

                thread_local Generator tlsG;
                thread_local bool tlsInited = false;
                thread_local int tlsMc = -1;
                thread_local uint64_t tlsSeed = ~0ull;
                if (!tlsInited || tlsMc != mcVer)
                {
                    setupGenerator(&tlsG, mcVer, FORCE_OCEAN_VARIANTS);
                    tlsInited = true;
                    tlsMc = mcVer;
                    tlsSeed = ~0ull;
                }
                if (tlsSeed != seedVal)
                {
                    applySeed(&tlsG, DIM_OVERWORLD, seedVal);
                    tlsSeed = seedVal;
                }

                const int originX = peak.bx - kAreaOrigin;
                const int originZ = peak.bz - kAreaOrigin;
                auto hot = findBiggestRiver<SearchConfig::PHASE1_CLIMATE_GRID_SCALE>(
                    &tlsG, originX, originZ, kAreaWindow, kAreaWindow,
                    minArea, 1.0, FilterMode::ClimateCoarse, riverWeight);
                const Res hotBest = bestOf(std::move(hot));
                if (hotBest.area >= minArea)
                {
                    auto precise = findBiggestRiver<1>(
                        &tlsG, originX, originZ, kAreaWindow, kAreaWindow,
                        minArea, 1.0, FilterMode::PreciseBiome, riverWeight);
                    const Res preciseBest = bestOf(std::move(precise));
                    if (preciseBest.area >= minArea)
                        found.addResult(preciseBest);
                }

                if (progress)
                {
                    progress->current.fetch_add(1);
                    progress->chunkInRunning.fetch_sub(1);
                }
            });
        }
    }
    return found.getAllResults();
}

std::vector<Res> runLargePitSearch(Generator *g, int minArea, float riverWeight, int threads, Progress *progress)
{
    if (threads < 1)
        threads = 1;
    const DoublePerlinNoise *weird = &g->bn.climate[NP_WEIRDNESS];
    verifyWeirdnessUnits(weird);
    if (pitPausedOrStopped(progress))
        return {};

    const std::vector<PitSeed> seeds = scanA0(weird, progress, threads);
    if (progress && progress->try_stop.load())
        return {};
    const std::vector<PitExtreme> extremes = refineAChain(weird, seeds, progress);
    if (progress && progress->try_stop.load())
        return {};
    const std::vector<PitPeak> peaks = scanBChain(weird, extremes, progress, threads);
    if (progress && progress->try_stop.load())
        return {};
    const std::vector<PitPeak> passed = filterContinentalness(&g->bn.climate[NP_CONTINENTALNESS], peaks, progress);
    if (progress && progress->try_stop.load())
        return {};
    (void) riverWeight;
    return scoreBiomes(g, passed, minArea, 0.0f, threads, progress);
}
