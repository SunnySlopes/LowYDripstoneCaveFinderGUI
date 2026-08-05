#include "cubiomes/generator.h"
#include "cubiomes/biomes.h"
#include <vector>
#include <algorithm>
#include <queue>
#include <iostream>
#include <chrono>
#include "Thread.h"
#include "BiomeSampler.h"
#include "SearchConfig.h"
#include <functional>
#include <ranges>
#include <unordered_map>
#include <cmath>
#include <cstdint>
#include <thread>

enum class FilterMode {
    WeirdnessOnly,
    ContinentalnessOnly,
    ContThenWeird,
    WeirdThenCont,
    ClimateCoarse,
    PreciseBiome
};

static FilterMode phase1CoarseToFilter(SearchConfig::Phase1CoarseFilter f)
{
    switch (f)
    {
    case SearchConfig::Phase1CoarseFilter::ContinentalnessOnly:
        return FilterMode::ContinentalnessOnly;
    case SearchConfig::Phase1CoarseFilter::ContThenWeird:
        return FilterMode::ContThenWeird;
    case SearchConfig::Phase1CoarseFilter::WeirdThenCont:
        return FilterMode::WeirdThenCont;
    case SearchConfig::Phase1CoarseFilter::WeirdnessOnly:
    default:
        return FilterMode::WeirdnessOnly;
    }
}

struct RingMask {
    struct Row { int dz; int out; int in; };
    std::vector<Row> rows;
    int R_out = 0;
};

template<int scale>
static const RingMask &getRingMask()
{
    static const RingMask mask = [] {
        RingMask m;
        m.R_out = 128 / scale;
        const int R_in = 24 / scale;
        std::vector<int> dxOut(2 * m.R_out + 1);
        std::vector<int> dxIn(2 * R_in + 1);
        for (int dz = -m.R_out; dz <= m.R_out; dz++)
            dxOut[dz + m.R_out] = (int) std::floor(std::sqrt((double) m.R_out * m.R_out - dz * dz));
        for (int dz = -R_in; dz <= R_in; dz++)
            dxIn[dz + R_in] = (int) std::floor(std::sqrt((double) R_in * R_in - dz * dz));
        m.rows.reserve(2 * m.R_out + 1);
        for (int dz = -m.R_out; dz <= m.R_out; dz++)
        {
            RingMask::Row row{};
            row.dz = dz;
            row.out = dxOut[dz + m.R_out];
            row.in = (std::abs(dz) <= R_in) ? dxIn[dz + R_in] : -1;
            m.rows.push_back(row);
        }
        return m;
    }();
    return mask;
}

static inline int coarseCellValue(Generator *g, int worldX, int worldZ, FilterMode mode)
{
    const int nx = worldX / 4;
    const int nz = worldZ / 4;
    if (mode == FilterMode::WeirdnessOnly)
        return passCaveWeirdness(&g->bn, nx, nz, COARSE_SAMPLE_FLAGS) ? 1 : 0;
    if (mode == FilterMode::ContinentalnessOnly)
        return passContinentalnessPartial(&g->bn, nx, nz) ? 1 : 0;
    if (mode == FilterMode::ContThenWeird)
    {
        if (!passContinentalnessPartial(&g->bn, nx, nz))
            return 0;
        return passCaveWeirdness(&g->bn, nx, nz, COARSE_SAMPLE_FLAGS) ? 1 : 0;
    }
    if (mode == FilterMode::WeirdThenCont)
    {
        if (!passCaveWeirdness(&g->bn, nx, nz, COARSE_SAMPLE_FLAGS))
            return 0;
        return passContinentalnessPartial(&g->bn, nx, nz) ? 1 : 0;
    }

    return passCoarseCaveCell(&g->bn, nx, nz, COARSE_SAMPLE_FLAGS) ? 1 : 0;
}

/** Build C>thr mask at `scale`, then Chebyshev-dilate by `dilate` tiles. */
static std::vector<uint8_t> buildDilatedContPrefilterMask(
    Generator *g, int startX, int startZ, int sx, int sz,
    int scale, double thr, int dilate)
{
    const int W = sx / scale;
    const int H = sz / scale;
    std::vector<uint8_t> keep((size_t) std::max(0, W) * std::max(0, H), 0);
    if (W <= 0 || H <= 0)
        return keep;

    for (int z = 0; z < H; z++)
    {
        const int worldZ = startZ + z * scale + scale / 2;
        for (int x = 0; x < W; x++)
        {
            const int worldX = startX + x * scale + scale / 2;
            if (passContinentalnessPartialThr(&g->bn, worldX / 4, worldZ / 4, thr))
                keep[(size_t) z * W + x] = 1;
        }
    }

    if (dilate <= 0)
        return keep;

    std::vector<uint8_t> out = keep;
    for (int z = 0; z < H; z++)
    {
        for (int x = 0; x < W; x++)
        {
            if (!keep[(size_t) z * W + x])
                continue;
            for (int dz = -dilate; dz <= dilate; dz++)
            {
                for (int dx = -dilate; dx <= dilate; dx++)
                {
                    const int nx = x + dx;
                    const int nz = z + dz;
                    if (nx < 0 || nz < 0 || nx >= W || nz >= H)
                        continue;
                    out[(size_t) nz * W + nx] = 1;
                }
            }
        }
    }
    return out;
}

static inline bool contPrefilterAllows(
    const std::vector<uint8_t> &mask, int maskW, int maskH,
    int startX, int startZ, int preScale, int worldX, int worldZ)
{
    if (mask.empty() || maskW <= 0 || maskH <= 0 || preScale <= 0)
        return true;
    const int tx = (worldX - startX) / preScale;
    const int tz = (worldZ - startZ) / preScale;
    if (tx < 0 || tz < 0 || tx >= maskW || tz >= maskH)
        return false;
    return mask[(size_t) tz * maskW + tx] != 0;
}

static void fillPreciseGrid(Generator *g, int startX, int startZ, int W, int H,
                            std::vector<int> &rawRiver, std::vector<int> &rawCave)
{
    fillPreciseWindow(g, startX, startZ, W, H, rawRiver, rawCave);
}

struct Point {
    int x = 0;
    int y = 0;
    std::strong_ordering operator<=>(const Point &) const = default;
};

struct Res {
    Point point;
    int area = 0;
    int caveArea = 0;
    int riverArea = 0;

    std::strong_ordering operator<=>(const Res &other) const noexcept
    {
        if (auto c = area <=> other.area; c != 0) return c;
        return point <=> other.point;
    }

    Res() = default;
    Res(Point p, int total, int cave, int river = 0)
        : point(p), area(total), caveArea(cave), riverArea(river) {}
};

struct Progress {
    std::atomic_int current{0};
    std::atomic_int total{0};
    std::atomic_int chunkInRunning{0};
    std::atomic_int phase1{0};
    std::atomic_bool try_pause{false};
    std::atomic_bool try_stop{false};
};

template<int scale>
std::vector<Res> findBiggestRiver(
    Generator *g,
    int startX, int startZ,
    int sx, int sz,
    int min,
    double f,
    FilterMode mode,
    float riverWeight = 0.7f,
    const std::vector<uint8_t> *contPrefilter = nullptr,
    int contPrefilterScale = 0) noexcept
{
    std::vector<Res> result;
    const int W = sx / scale;
    const int H = sz / scale;
    if (W <= 0 || H <= 0) return result;

    const int preW = (contPrefilter && contPrefilterScale > 0) ? (sx / contPrefilterScale) : 0;
    const int preH = (contPrefilter && contPrefilterScale > 0) ? (sz / contPrefilterScale) : 0;

    const int stride = W + 1;
    std::vector<int> rawRiver((size_t) W * H, 0);
    std::vector<int> prefixRiver((size_t) (W + 1) * (H + 1), 0);

    // Cave grids only needed for precise (scale==1) path.
    std::vector<int> rawCave;
    std::vector<int> prefixCave;
    if constexpr (scale == 1)
    {
        rawCave.assign((size_t) W * H, 0);
        prefixCave.assign((size_t) (W + 1) * (H + 1), 0);
    }

#define RAWR(x,z) rawRiver[(size_t)(x) + (size_t)(z) * W]
#define RAWC(x,z) rawCave[(size_t)(x) + (size_t)(z) * W]
#define ARRR(x,z) prefixRiver[(size_t)(x) + (size_t)(z) * stride]
#define ARRC(x,z) prefixCave[(size_t)(x) + (size_t)(z) * stride]

    const int R_out = 128 / scale;
    const int occTile = std::max(8, R_out);
    const int oW = (W + occTile - 1) / occTile;
    const int oH = (H + occTile - 1) / occTile;
    std::vector<uint8_t> occ((size_t) std::max(0, oW) * std::max(0, oH), 0);

    auto markOcc = [&](int x, int z) {
        if (oW <= 0 || oH <= 0) return;
        occ[(size_t) (z / occTile) * oW + (size_t) (x / occTile)] = 1;
    };

    if constexpr (scale > 1)
    {
        for (int z = 0; z < H; z++)
        {
            const int worldZ = startZ + z * scale + scale / 2;
            for (int x = 0; x < W; x++)
            {
                const int worldX = startX + x * scale + scale / 2;
                if (contPrefilter &&
                    !contPrefilterAllows(*contPrefilter, preW, preH,
                                         startX, startZ, contPrefilterScale, worldX, worldZ))
                {
                    RAWR(x, z) = 0;
                    continue;
                }
                const int v = coarseCellValue(g, worldX, worldZ, mode);
                RAWR(x, z) = v;
                if (v)
                    markOcc(x, z);
            }
        }
    } else
    {
        fillPreciseGrid(g, startX, startZ, W, H, rawRiver, rawCave);
        for (int z = 0; z < H; z++)
        {
            for (int x = 0; x < W; x++)
            {
                if (RAWR(x, z) || RAWC(x, z))
                    markOcc(x, z);
            }
        }
    }

    for (int z = 1; z <= H; z++)
    {
        for (int x = 1; x <= W; x++)
        {
            ARRR(x, z) = RAWR(x - 1, z - 1) + ARRR(x - 1, z) + ARRR(x, z - 1) - ARRR(x - 1, z - 1);
            if constexpr (scale == 1)
            {
                ARRC(x, z) = RAWC(x - 1, z - 1) + ARRC(x - 1, z) + ARRC(x, z - 1) - ARRC(x - 1, z - 1);
            }
        }
    }

    auto occAnyInRect = [&](int x0, int x1, int z0, int z1) -> bool {
        if (oW <= 0 || oH <= 0) return true;
        const int tx0 = std::max(0, x0 / occTile);
        const int tx1 = std::min(oW - 1, x1 / occTile);
        const int tz0 = std::max(0, z0 / occTile);
        const int tz1 = std::min(oH - 1, z1 / occTile);
        if (tx0 > tx1 || tz0 > tz1) return false;
        for (int tz = tz0; tz <= tz1; tz++)
        {
            for (int tx = tx0; tx <= tx1; tx++)
            {
                if (occ[(size_t) tz * oW + (size_t) tx])
                    return true;
            }
        }
        return false;
    };

    struct CandidateArea {
        int area;
        int caveArea;
        int riverArea;
        int startX;
        int startZ;

        bool operator<(const CandidateArea &other) const noexcept
        {
            if (area != other.area) return area < other.area;
            if (startX != other.startX) return startX > other.startX;
            return startZ > other.startZ;
        }
    };

    std::priority_queue<CandidateArea> pq;
    const auto &ring = getRingMask<scale>();

    auto ringSum = [&](const std::vector<int> &pref, int cx, int cz) -> int {
        int area = 0;
        for (const auto &m: ring.rows)
        {
            const int row = cz + m.dz;
            const int L = cx - m.out;
            const int R = cx + m.out;
            if (m.in == -1)
            {
                area += pref[(size_t) (R + 1) + (size_t) (row + 1) * stride]
                      - pref[(size_t) L + (size_t) (row + 1) * stride]
                      - pref[(size_t) (R + 1) + (size_t) row * stride]
                      + pref[(size_t) L + (size_t) row * stride];
            } else
            {
                const int Lin = cx - m.in;
                const int Rin = cx + m.in;
                area += pref[(size_t) Lin + (size_t) (row + 1) * stride]
                      - pref[(size_t) L + (size_t) (row + 1) * stride]
                      - pref[(size_t) Lin + (size_t) row * stride]
                      + pref[(size_t) L + (size_t) row * stride];
                area += pref[(size_t) (R + 1) + (size_t) (row + 1) * stride]
                      - pref[(size_t) (Rin + 1) + (size_t) (row + 1) * stride]
                      - pref[(size_t) (R + 1) + (size_t) row * stride]
                      + pref[(size_t) (Rin + 1) + (size_t) row * stride];
            }
        }
        return area;
    };

    CandidateArea maxA{0, 0, 0, 0, 0};

    for (int cz = R_out; cz < H - R_out; cz++)
    {
        for (int cx = R_out; cx < W - R_out; cx++)
        {
            // Skip ring centers whose bounding box has no positive cells
            if (!occAnyInRect(cx - R_out, cx + R_out, cz - R_out, cz + R_out))
                continue;

            int worldTotal;
            int worldCave;
            int worldRiver;

            if constexpr (scale == 1)
            {
                const int sumR = ringSum(prefixRiver, cx, cz);
                const int sumC = ringSum(prefixCave, cx, cz);
                worldRiver = (sumR + 1) / 3;
                worldCave = (sumC + 1) / 3;
                worldTotal = worldCave + (int) (worldRiver * riverWeight);
            } else
            {
                const int sum = ringSum(prefixRiver, cx, cz);
                worldTotal = sum * scale * scale;
                worldCave = 0;
                worldRiver = 0;
            }

            if (worldTotal >= maxA.area * f && worldTotal >= min)
            {
                const int worldX = startX + cx * scale;
                const int worldZ = startZ + cz * scale;
                pq.push({worldTotal, worldCave, worldRiver, worldX, worldZ});
                if (worldTotal > maxA.area)
                {
                    maxA.area = worldTotal;
                    maxA.caveArea = worldCave;
                    maxA.riverArea = worldRiver;
                }
            }
        }
    }

    while (!pq.empty())
    {
        if (const auto &ra = pq.top(); ra.area >= maxA.area * f)
        {
            result.emplace_back(Point{ra.startX, ra.startZ}, ra.area, ra.caveArea, ra.riverArea);
            if (f == 1.0) break;
        }
        pq.pop();
    }

#undef RAWR
#undef RAWC
#undef ARRR
#undef ARRC
    return result;
}

void findBiggestRiverParallelPool(
    ThreadSafeResults<Res> &globalResults,
    Generator *g,
    int startX, int startZ,
    int sx, int sz,
    int minArea,
    Progress *progress = nullptr,
    int numThreads = static_cast<int>(std::thread::hardware_concurrency())
)
{
    ThreadPool pool(numThreads);
    const int chunkSize = 4096 * 2;
    const int tile = SearchConfig::CANDIDATE_TILE_BLOCKS;
    const int overlap = tile;
    const int step = chunkSize - overlap;
    const int subR = SearchConfig::SUBSEARCH_RADIUS_BLOCKS;
    const int subSz = SearchConfig::SUBSEARCH_SIZE_BLOCKS;
    std::atomic<int> completedChunks{0};
    int totalChunks = 0;

#ifndef DRIPSTONECAVE_FINDER_JNI_LIB
    auto startTime = std::chrono::high_resolution_clock::now();
#endif

    // Count chunks first so UI can show total/ETA before enqueue finishes
    for (int x = 0; x < sx; x += step)
    {
        for (int z = 0; z < sz; z += step)
        {
            int currentSx = std::min(chunkSize, sx - x);
            int currentSz = std::min(chunkSize, sz - z);
            if (currentSx >= tile && currentSz >= tile)
                totalChunks++;
        }
    }

    if (progress)
        progress->total.store(totalChunks);

    const uint64_t seedVal = g->seed;

    for (int x = 0; x < sx; x += step)
    {
        for (int z = 0; z < sz; z += step)
        {
            if (progress && progress->try_stop.load())
                return;

            int currentSx = std::min(chunkSize, sx - x);
            int currentSz = std::min(chunkSize, sz - z);

            if (currentSx >= tile && currentSz >= tile)
            {
                pool.enqueue([&, x, z, currentSx, currentSz, seedVal]() {
                    if (progress)
                    {
                        if (progress->try_stop.load()) return;
                        while (progress->try_pause.load())
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(200));
                            if (progress->try_stop.load()) return;
                        }
                        progress->chunkInRunning.fetch_add(1);
                    }

                    // Per-worker Generator: setup once, re-seed when seed changes.
                    thread_local Generator tlsG;
                    thread_local bool tlsInited = false;
                    thread_local uint64_t tlsSeed = ~0ull;
                    if (!tlsInited)
                    {
                        setupGenerator(&tlsG, MC_1_21_3, FORCE_OCEAN_VARIANTS);
                        tlsInited = true;
                    }
                    if (tlsSeed != seedVal)
                    {
                        applySeed(&tlsG, DIM_OVERWORLD, seedVal);
                        tlsSeed = seedVal;
                    }

                    const int csx = startX + x;
                    const int csz = startZ + z;

                    const auto contMask = buildDilatedContPrefilterMask(
                        &tlsG, csx, csz, currentSx, currentSz,
                        SearchConfig::PHASE1_CONT_PREFILTER_SCALE,
                        SearchConfig::PHASE1_CONT_PREFILTER_THRESHOLD,
                        SearchConfig::PHASE1_CONT_PREFILTER_DILATE);

                    auto finishChunk = [&]() {
                        int completed = completedChunks.fetch_add(1) + 1;
                        if (progress)
                        {
                            progress->current.store(completed);
                            progress->chunkInRunning.fetch_sub(1);
                        }
#ifndef DRIPSTONECAVE_FINDER_JNI_LIB
                        if (completed % 500 == 0)
                        {
                            auto currentTime = std::chrono::high_resolution_clock::now();
                            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                currentTime - startTime).count();
                            double speed = static_cast<double>(completed) / elapsed * 1000;
                            std::cout << "Progress: " << completed << "/" << totalChunks
                                    << " (" << int(completed * 100.0 / totalChunks)
                                    << "%) - " << speed << " chunks/sec\n";
                        }
#endif
                    };

                    // Empty Cont mask: no Phase1 work in this chunk.
                    if (std::none_of(contMask.begin(), contMask.end(),
                                     [](uint8_t v) { return v != 0; }))
                    {
                        finishChunk();
                        return;
                    }

                    auto blockResultsX16 = findBiggestRiver<SearchConfig::PHASE1_WEIRDNESS_GRID_SCALE>(
                        &tlsG, csx, csz, currentSx, currentSz,
                        minArea, 0.8, phase1CoarseToFilter(SearchConfig::PHASE1_COARSE_FILTER),
                        0.7f, &contMask, SearchConfig::PHASE1_CONT_PREFILTER_SCALE);

                    const int bx = currentSx / tile + 2;
                    const int bz = currentSz / tile + 2;
                    std::vector<Res> flags((size_t) bx * bz);
                    for (const auto &it: blockResultsX16)
                    {
                        int x2 = (it.point.x - csx) / tile;
                        int z2 = (it.point.y - csz) / tile;
                        if (x2 >= 0 && x2 < bx && z2 >= 0 && z2 < bz)
                        {
                            auto &itf = flags[(size_t) x2 + (size_t) bx * z2];
                            if (itf.area < it.area) itf = it;
                        }
                    }

                    std::vector<Res> pqX16;
                    for (auto &kv: flags)
                        if (kv.area > 0) pqX16.push_back(kv);
                    std::ranges::sort(pqX16, [](const Res &a, const Res &b) { return a.area > b.area; });

                    std::unordered_map<uint64_t, Res> blockResultsX4;
                    int max = 0;
                    for (auto &res: pqX16)
                    {
                        auto subResults = findBiggestRiver<SearchConfig::PHASE1_CLIMATE_GRID_SCALE>(
                            &tlsG,
                            res.point.x - subR, res.point.y - subR,
                            subSz, subSz,
                            minArea, 1.0, FilterMode::ClimateCoarse);

                        if (subResults.empty()) break;
                        if (subResults[0].area < max * 0.9) break;

                        auto &r = subResults[0];
                        uint64_t key = ((uint64_t) (uint32_t) r.point.x << 32) | (uint32_t) r.point.y;
                        auto it = blockResultsX4.find(key);
                        if (it == blockResultsX4.end())
                            blockResultsX4.emplace(key, r);
                        else if (it->second.area < r.area)
                            it->second = r;

                        if (subResults[0].area > max) max = subResults[0].area;
                    }

                    std::vector<Res> filteredResults;
                    for (const auto &result: blockResultsX4 | std::views::values)
                    {
                        int relX = result.point.x - csx;
                        int relZ = result.point.y - csz;
                        if (relX > overlap / 2 && relX < currentSx - overlap / 2 &&
                            relZ > overlap / 2 && relZ < currentSz - overlap / 2 &&
                            result.area > 0)
                        {
                            filteredResults.push_back(result);
                        }
                    }
                    std::ranges::sort(filteredResults, [](const Res &a, const Res &b) { return a.area > b.area; });
                    if (!filteredResults.empty())
                        globalResults.addResults(filteredResults);

                    finishChunk();
                });
            }
        }
    }

#ifndef DRIPSTONECAVE_FINDER_JNI_LIB
    std::cout << "Submitted " << totalChunks << " chunks to thread pool\n";
#endif
}

#ifndef DRIPSTONECAVE_FINDER_JNI_LIB
int main(int argc, char **argv)
{
    (void) argc;
    int64_t seed = -8180004378910677489;
    int px = 0, pz = 0, d = 4096;
    std::cout << "seed: ";
    std::cin >> seed;
    std::cout << "center_x: ";
    std::cin >> px;
    std::cout << "center_z: ";
    std::cin >> pz;
    std::cout << "r: ";
    std::cin >> d;

    int startX = px - d;
    int startZ = pz - d;
    int xRange = 2 * d;
    int zRange = 2 * d;
    int minArea = 40000;
    const char *outFile = "out1.txt";
    int outLimit = 1000;

    Generator g;
    setupGenerator(&g, MC_1_21_3, FORCE_OCEAN_VARIANTS);
    applySeed(&g, DIM_OVERWORLD, seed);

    ThreadSafeResults<Res> globalResults;
    findBiggestRiverParallelPool(globalResults, &g, startX, startZ, xRange, zRange, minArea, nullptr);

    auto res = globalResults.getAllResults();
    std::ranges::sort(res, [](const Res &a, const Res &b) { return a.area > b.area; });

    int max = 0;
    std::vector<Res> finallyResults;
    for (const auto &it: res)
    {
        auto temp = findBiggestRiver<1>(
            &g,
            it.point.x - SearchConfig::REFINE_HALF_WINDOW,
            it.point.y - SearchConfig::REFINE_HALF_WINDOW,
            SearchConfig::REFINE_WINDOW_BLOCKS, SearchConfig::REFINE_WINDOW_BLOCKS,
            1, 1.0, FilterMode::PreciseBiome);
        if (!temp.empty() && temp[0].area > max * 0.90)
        {
            finallyResults.push_back(temp[0]);
            if (temp[0].area > max) max = temp[0].area;
        } else
        {
            break;
        }
    }

    FILE *fp = fopen(outFile, "w");
    if (fp)
    {
        fprintf(fp, "Cave Analysis Results (Seed: %lld)\n", (long long) seed);
        fprintf(fp, "Search Area: X=%d to %d, Z=%d to %d\n",
                startX, startX + xRange, startZ, startZ + zRange);
        fprintf(fp, "========================================\n");
    }

    int count = 0;
    for (const auto &it: finallyResults)
    {
        std::cout << "x:" << it.point.x << " y:" << it.point.y
                << "  Area:" << it.area << " cave:" << it.caveArea
                << " river:" << it.riverArea << std::endl;
        if (fp)
        {
            fprintf(fp, "x:%d y:%d  Area:%d cave:%d river:%d\n",
                    it.point.x, it.point.y, it.area, it.caveArea, it.riverArea);
        }
        count++;
        if (count > outLimit) break;
    }
    if (fp)
    {
        fprintf(fp, "\nTotal results: %d\n", count);
        fclose(fp);
        std::cout << "\nResults saved to: " << outFile << std::endl;
    }
    return 0;
}
#endif
