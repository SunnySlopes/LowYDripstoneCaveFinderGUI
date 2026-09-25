#include "OctaveFieldCache.h"

#include "cubiomes/biomes.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <thread>
#include <vector>

static const OctaveACache *g_activeOctaveACache = nullptr;

void setActiveOctaveACache(const OctaveACache *cache)
{
    g_activeOctaveACache = cache;
}

const OctaveACache *getActiveOctaveACache()
{
    return g_activeOctaveACache;
}

static inline int floorDivPositive(int a, int b)
{
    /* floor div for potentially negative a, positive b */
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0)))
        --q;
    return q;
}

static inline int modPositive(int a, int m)
{
    int r = a % m;
    if (r < 0)
        r += m;
    return r;
}

float OctaveACache::Field::atClimate(int bx, int bz) const
{
    if (n <= 0 || data.empty() || sampleScale <= 0)
        return 0.0f;
    const int stepClim = sampleScale / 4; /* world scale → climate */
    const int half = stepClim / 2;
    /* Map climate to lattice index of prefilter centers (…, half, half+step, …). */
    const int ix = modPositive(floorDivPositive(bx - half, stepClim), n);
    const int iz = modPositive(floorDivPositive(bz - half, stepClim), n);
    return data[(size_t) iz * (size_t) n + (size_t) ix];
}

static double sampleOctALive(const PerlinNoise *p, double x, double z)
{
    const double lf = p->lacunarity;
    const double ax = maintainPrecision(x * lf);
    const double az = maintainPrecision(z * lf);
    return p->amplitude * samplePerlin(p, ax, 0.0, az, 0, 0);
}

bool buildOctaveACache(
    OctaveACache *out,
    const BiomeNoise *bn,
    int contScale,
    int weirdScale,
    int numThreads,
    std::atomic_int *progressCurrent,
    std::atomic_int *progressTotal,
    const std::atomic_bool *tryPause,
    const std::atomic_bool *tryStop)
{
    if (!out || !bn || contScale <= 0 || weirdScale <= 0)
        return false;

    out->ready = false;
    const DoublePerlinNoise *cont = &bn->climate[NP_CONTINENTALNESS];
    const DoublePerlinNoise *ridge = &bn->climate[NP_WEIRDNESS];

    if (cont->octA.octcnt < OctaveACache::CONT_OCT || ridge->octA.octcnt < OctaveACache::RIDGE_OCT)
        return false;

    /* Count total cells for progress. */
    long long totalCells = 0;
    for (int i = 0; i < OctaveACache::CONT_OCT; i++)
    {
        const int n = kContAPeriodWorld[i] / contScale;
        totalCells += (long long) n * n;
    }
    for (int i = 0; i < OctaveACache::RIDGE_OCT; i++)
    {
        const int n = kRidgeAPeriodWorld[i] / weirdScale;
        totalCells += (long long) n * n;
    }
    if (progressTotal)
        progressTotal->store((int) std::min(totalCells, (long long) INT32_MAX));
    if (progressCurrent)
        progressCurrent->store(0);

    auto respectPauseStop = [&]() -> bool {
        if (tryStop && tryStop->load())
            return false;
        if (tryPause)
        {
            while (tryPause->load())
            {
                if (tryStop && tryStop->load())
                    return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        return true;
    };

    int threads = numThreads > 0 ? numThreads : 1;
    if (threads < 1)
        threads = 1;

    /* Fill Cont A fields (parallelize within largest fields). */
    for (int oi = 0; oi < OctaveACache::CONT_OCT; oi++)
    {
        if (!respectPauseStop())
            return false;
        OctaveACache::Field *f = &out->contA[oi];
        const PerlinNoise *p = cont->octA.octaves + oi;
        f->periodWorld = kContAPeriodWorld[oi];
        f->sampleScale = contScale;
        f->n = f->periodWorld / contScale;
        f->data.assign((size_t) f->n * (size_t) f->n, 0.0f);
        const int n = f->n;
        const int stepClim = contScale / 4;
        const int half = stepClim / 2;

        std::vector<std::thread> pool;
        std::atomic_int nextRow{0};
        for (int t = 0; t < threads; t++)
        {
            pool.emplace_back([&, p, n, stepClim, half]() {
                for (;;)
                {
                    if (tryStop && tryStop->load())
                        return;
                    const int iz = nextRow.fetch_add(1);
                    if (iz >= n)
                        return;
                    if (tryPause)
                    {
                        while (tryPause->load())
                        {
                            if (tryStop && tryStop->load())
                                return;
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        }
                    }
                    const int bz = iz * stepClim + half;
                    for (int ix = 0; ix < n; ix++)
                    {
                        const int bx = ix * stepClim + half;
                        f->data[(size_t) iz * (size_t) n + (size_t) ix] =
                            (float) sampleOctALive(p, (double) bx, (double) bz);
                    }
                    if (progressCurrent)
                        progressCurrent->fetch_add(n);
                }
            });
        }
        for (auto &th : pool)
            th.join();
        if (tryStop && tryStop->load())
            return false;
    }

    for (int oi = 0; oi < OctaveACache::RIDGE_OCT; oi++)
    {
        if (!respectPauseStop())
            return false;
        OctaveACache::Field *f = &out->ridgeA[oi];
        const PerlinNoise *p = ridge->octA.octaves + oi;
        f->periodWorld = kRidgeAPeriodWorld[oi];
        f->sampleScale = weirdScale;
        f->n = f->periodWorld / weirdScale;
        f->data.assign((size_t) f->n * (size_t) f->n, 0.0f);
        const int n = f->n;
        const int stepClim = weirdScale / 4;
        const int half = stepClim / 2;

        std::vector<std::thread> pool;
        std::atomic_int nextRow{0};
        for (int t = 0; t < threads; t++)
        {
            pool.emplace_back([&, p, n, stepClim, half]() {
                for (;;)
                {
                    if (tryStop && tryStop->load())
                        return;
                    const int iz = nextRow.fetch_add(1);
                    if (iz >= n)
                        return;
                    if (tryPause)
                    {
                        while (tryPause->load())
                        {
                            if (tryStop && tryStop->load())
                                return;
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        }
                    }
                    const int bz = iz * stepClim + half;
                    for (int ix = 0; ix < n; ix++)
                    {
                        const int bx = ix * stepClim + half;
                        f->data[(size_t) iz * (size_t) n + (size_t) ix] =
                            (float) sampleOctALive(p, (double) bx, (double) bz);
                    }
                    if (progressCurrent)
                        progressCurrent->fetch_add(n);
                }
            });
        }
        for (auto &th : pool)
            th.join();
        if (tryStop && tryStop->load())
            return false;
    }

    out->ready = true;
    if (progressCurrent && progressTotal)
        progressCurrent->store(progressTotal->load());
    return true;
}
