#include "sunnyslopes_lowydscavefinder_LowYDripstoneCaveFinderBridge.h"

#include <chrono>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "../../river_finder.cpp"

static std::mutex searchMutex;
static std::vector<Res> dedup(const std::vector<Res> &v)
{
    std::set<Point> seen;
    std::vector<Res> out;
    for (const auto &item: v)
        if (seen.insert(item.point).second)
            out.push_back(item);
    return out;
}

static void appendResult(std::vector<jint> &results, const Res &item)
{
    results.push_back(item.point.x);
    results.push_back(item.point.y);
    results.push_back(item.area);
    results.push_back(item.caveArea);
    results.push_back(item.riverArea);
}

static Progress progress{};
static ThreadSafeResults<Res> globalResults{};

static void resetProgressState()
{
    progress.chunkInRunning.store(0);
    progress.current.store(0);
    progress.total.store(0);
    progress.phase1.store(0);
    progress.try_pause.store(false);
    progress.try_stop.store(false);
}

JNIEXPORT jintArray JNICALL Java_sunnyslopes_lowydscavefinder_LowYDripstoneCaveFinderBridge_riverSearch
  (JNIEnv *env, jclass, jlong seed, jint startX, jint startZ,
   jint width, jint height, jint /*y*/, jint minArea, jfloat opV, jfloat riverWeight, jint numThreads)
{
    std::lock_guard<std::mutex> guard(searchMutex);

    if (riverWeight < 0.0f) riverWeight = 0.0f;
    if (riverWeight > 1.0f) riverWeight = 1.0f;
    (void) opV;
    Generator g;
    setupGenerator(&g, MC_1_21_3, FORCE_OCEAN_VARIANTS);
    applySeed(&g, DIM_OVERWORLD, (uint64_t) seed);

    globalResults.clear();
    resetProgressState();
    progress.phase1.store(1);

    int threads = numThreads > 0 ? numThreads : static_cast<int>(std::thread::hardware_concurrency());
    findBiggestRiverParallelPool(globalResults, &g, startX, startZ, width, height, minArea, &progress, threads);

    auto res = globalResults.getAllResults();
    if (progress.try_stop.load()) {
        resetProgressState();
        return nullptr;
    }

    progress.phase1.store(2);
    progress.total.store(static_cast<int>(res.size()));
    progress.current.store(0);
    progress.chunkInRunning.store(0);

    std::ranges::sort(res, [](const Res &a, const Res &b) { return a.area > b.area; });

    globalResults.clear();
    for (const auto &it: res)
    {
        while (progress.try_pause.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

        if (progress.try_stop.load()) {
            resetProgressState();
            return nullptr;
        }

        auto temp = findBiggestRiver<1>(
            &g, it.point.x - 160, it.point.y - 160, 320, 320,
            minArea, 1.0, FilterMode::PreciseBiome, riverWeight);

        if (!temp.empty() && temp[0].area >= minArea)
            globalResults.addResult(temp[0]);

        progress.current.fetch_add(1);
    }

    auto finalList = dedup(globalResults.getAllResults());
    globalResults.clear();
    globalResults.addResults(finalList);

    if (globalResults.empty()) {
        resetProgressState();
        return nullptr;
    }

    auto finallyResults = globalResults.getAllResults();
    progress.phase1.store(-1);

    std::vector<jint> results;
    results.reserve(finallyResults.size() * 5);
    for (const auto &item: finallyResults)
        appendResult(results, item);

    jintArray jResults = env->NewIntArray(static_cast<jsize>(results.size()));
    if (!jResults) {
        resetProgressState();
        return nullptr;
    }
    env->SetIntArrayRegion(jResults, 0, static_cast<jsize>(results.size()), results.data());
    resetProgressState();
    return jResults;
}

JNIEXPORT jintArray JNICALL Java_sunnyslopes_lowydscavefinder_LowYDripstoneCaveFinderBridge_getSearchProgress
  (JNIEnv *env, jclass)
{
    if (progress.total.load() <= 0)
        return nullptr;

    int status = 0;
    int phase = progress.phase1.load();
    int cur = progress.current.load();
    int tot = progress.total.load();
    if (phase == 1)
        status = progress.chunkInRunning.load() == 0 ? 1 : 0;
    else if (phase == 2)
        status = cur >= tot ? 2 : 1;

    jint data[6] = {
        cur, tot, phase, status,
        progress.try_pause.load() ? 1 : 0, progress.try_stop.load() ? 1 : 0
    };
    jintArray arr = env->NewIntArray(6);
    if (!arr)
        return nullptr;
    env->SetIntArrayRegion(arr, 0, 6, data);
    return arr;
}

JNIEXPORT jboolean JNICALL Java_sunnyslopes_lowydscavefinder_LowYDripstoneCaveFinderBridge_pause(JNIEnv *, jclass)
{
    progress.try_pause.store(true);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_sunnyslopes_lowydscavefinder_LowYDripstoneCaveFinderBridge_resume(JNIEnv *, jclass)
{
    progress.try_pause.store(false);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_sunnyslopes_lowydscavefinder_LowYDripstoneCaveFinderBridge_stop(JNIEnv *, jclass)
{
    progress.try_stop.store(true);
    return JNI_TRUE;
}

JNIEXPORT jintArray JNICALL Java_sunnyslopes_lowydscavefinder_LowYDripstoneCaveFinderBridge_getNowResult(JNIEnv *env, jclass)
{
    if (globalResults.empty())
        return nullptr;

    auto res = globalResults.getAllResults();
    std::ranges::sort(res, [](const Res &a, const Res &b) { return a.area > b.area; });

    std::vector<jint> results;
    results.reserve(res.size() * 5);
    for (const auto &item: res)
        appendResult(results, item);

    jintArray jResults = env->NewIntArray(static_cast<jsize>(results.size()));
    if (!jResults)
        return nullptr;
    env->SetIntArrayRegion(jResults, 0, static_cast<jsize>(results.size()), results.data());
    return jResults;
}
