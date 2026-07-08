#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#include "generator.h"
#include "generator.h"
#include "cubiomes/biomes.h"
#include <set>
#include <vector>
#include <algorithm>
#include  <queue>
#include <iostream>
#include <chrono>
#include "Thread.h"
#include <iostream>
#include <functional>

#include "finders.h"

#define AREA_SIZE 256// 搜索步长（不重叠）

/* ================== 河流判定 ================== */








template<typename Func>
void measure_time(Func func, const std::string& name = "Function") {
    auto start = std::chrono::high_resolution_clock::now();

    // 执行传入的函数
    func();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << name << " took: "
              << duration.count() << " microseconds ("
              << duration.count() / 1000.0 << " ms)" << std::endl;
}
static inline int isRiverBiome(int id)
{
    return (id == river)*0 + (id == dripstone_caves)*10;
}
static inline int isDripstone_caves(int id)
{

    return (id == dripstone_caves);
}

static inline int value(int id)
{
    return (id == river)*8 + (id == dripstone_caves)*10;
}


struct point {
    int x = 0;
    int y = 0;

};

struct Res {
    point point;
    int area = 0;
    bool operator<(const Res& other) const {
        // 首先按面积降序，面积相同时按坐标排序
        if (area != other.area) return area < other.area;
        return point.x*point.x + point.y*point.y < other.point.x*other.point.x+other.point.y*other.point.y;
    }

    Res() = default;

    Res(::point point, int area):point(point),area(area){};
};


std::vector<Res> findBiggestRiver(Generator *g, int startX, int startZ, int sx,int sz,int min,int scale = 4) {
    std::vector<Res> result;

    std::vector<std::vector<unsigned char>> flags(sx/16,std::vector<unsigned char>(sz/16,0));

    int h  = sx/16;
    int w = sz/16;
    for (int i = 0;i<h;i++)
    {
        for (int j = 0;j<w;j++)
        {
            Pos pos;
            if (    getStructurePos(Geode,MC_1_21_3,g->seed,startX/16 + i,startZ/16 + j,& pos))
            {
                if (isViableStructurePos(Geode,g,pos.x,pos.z,0))
                {
                    flags[i][j] = 1;
                };
            }
        }
    }

    std::vector<std::vector<int>> pre(h + 1, std::vector<int>(w + 1, 0));
    for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; ++j) {
            pre[i + 1][j + 1] = pre[i][j + 1] + pre[i + 1][j] - pre[i][j] + flags[i][j];
        }
    }

    int maxSum = -1;
    int bestX = 0;
    int bestY  = 0;
    // 遍历所有可能的16x16子矩阵的左上角
    for (int i = 0; i <= h - 16; ++i) {
        for (int j = 0; j <= w - 16; ++j) {
            int sum = pre[i + 16][j + 16] - pre[i][j + 16] - pre[i + 16][j] + pre[i][j];
            if (sum > maxSum) {
                maxSum = sum;
                bestX = i;
                bestY = j;
            }
        }
    }

    if (maxSum > 30)
    {
        return {Res({startX + bestX*16 + 128,startZ + bestY*16 + 128},maxSum)};
    }
    return {};
}

/* ================== 主程序 ================== */



void findBiggestRiverParallelPool(
    ThreadSafeResults<Res> & globalResults,
    Generator* g,
    int startX,
    int startZ,
    int sx,
    int sz,
    int minArea,
    int numThreads = std::thread::hardware_concurrency()
) {



    ThreadPool pool(numThreads);

    constexpr int chunkSize = 4096;
    const int overlap = 256;
    std::atomic<int> completedChunks{0};
    int totalChunks = 0;

    auto startTime = std::chrono::high_resolution_clock::now();

    // 提交所有任务到线程池
    for (int x = 0; x < sx; x += chunkSize - overlap) {
        for (int z = 0; z < sz; z += chunkSize - overlap) {
            int currentSx = std::min(chunkSize, sx - x);
            int currentSz = std::min(chunkSize, sz - z);

            if (currentSx >= 256 && currentSz >= 256) {
                totalChunks++;

                // 为每个块创建独立的任务
                pool.enqueue([&, x, z, currentSx, currentSz]() {
                    // 创建线程本地Generator
                    Generator localG;
                    memcpy(&localG, g, sizeof(Generator));

                    auto blockResults = findBiggestRiver(
                        &localG,
                        startX + x,
                        startZ + z,
                        currentSx,
                        currentSz,
                        minArea
                    );



                    // 过滤重叠区域的结果
                    std::vector<Res> filteredResults;
                    for (const auto& result : blockResults) {
                        int relX = result.point.x - (startX + x);
                        int relZ = result.point.y - (startZ + z);

                        if (relX > overlap/2 && relX < currentSx - overlap/2 &&
                            relZ > overlap/2 && relZ < currentSz - overlap/2) {
                            filteredResults.emplace_back(result.point, result.area);
                        }
                    }

                    // 添加结果


                    int count = 0;
                    if (!filteredResults.empty()) {
                        Res res = filteredResults[0];
                        globalResults.addResult(res);
                    }

                    // 更新进度
                    int completed = completedChunks.fetch_add(1) + 1;
                    if (completed % 100 == 0) {
                        auto currentTime = std::chrono::high_resolution_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                            currentTime - startTime
                        ).count();
                        double speed = static_cast<double>(completed) / elapsed * 1000;
                        std::cout << "Progress: " << completed << "/" << totalChunks
                                  << " chunks (" << static_cast<int>(completed * 100.0 / totalChunks)
                                  << "%) - " << speed << " chunks/sec\n";
                    }
                    if (completed%500 == 0)
                    {
                        const auto& r = globalResults.get();
                        std::cout<<"Now Max: ["<<r.point.x<<", "<<r.point.y<<"] value:"<<r.area<<"\n";
                    }
                });
            }
        }
    }

    std::cout << "Submitted " << totalChunks << " chunks to thread pool\n";

    // 等待所有任务完成（线程池析构时会自动等待）
    // ThreadPool析构时会自动等待所有任务完成

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime
    );

    std::cout << "Thread pool processing took: " << duration.count()
              << "ms for " << totalChunks << " chunks\n";
    return;
}












int main(int argc, char **argv)
{

    int d = 256;

    // 正确解析所有参数
    int64_t seed = 7103583996044705691;
    int startX = -d;
    int startZ =-d;
    int endX = -d;
    int endZ =-d;
    int px,py;

    std::cin>>px;
    std::cin>>py;
    startX = px - 2000000;
    startZ = py - 2000000;
    int xRange = 4000000;
    int zRange = 4000000;

    int minArea = 10;
    const char* outFile = "out1.txt";
    int outLimit = 1000;


    if (xRange <= 0 || zRange <= 0) {
        fprintf(stderr, "Error: x_range and z_range must be positive\n");
        return 1;
    }



    Generator g;
    setupGenerator(&g, MC_1_21, FORCE_OCEAN_VARIANTS);

    applySeed(&g, DIM_OVERWORLD, seed);

    ThreadSafeResults<Res> globalResults;

    // 使用从命令行获取的参数
    findBiggestRiverParallelPool(globalResults, &g, startX, startZ, xRange, zRange,minArea );

    // 输出到文件（如果需要）
    FILE* fp = fopen(outFile, "w");
    if (fp) {
        fprintf(fp, "River Analysis Results (Seed: %lld)\n", (long long)seed);
        fprintf(fp, "Search Area: X=%d to %d, Z=%d to %d\n",
                startX, startX + xRange, startZ, startZ + zRange);
        fprintf(fp, "========================================\n");
    }

    int count = 0;
    for (const auto& it : globalResults.getAllResults()) {
        std::cout << "x:" << it.point.x << " y:" << it.point.y
                  << "  Area:" << it.area << std::endl;

        if (fp) {
            fprintf(fp, "x:%d y:%d  Area:%d\n",
                    it.point.x, it.point.y, it.area);
        }
        count++;
        if (outLimit >0 && count > outLimit) {
            break;
        }
    }

    if (fp) {
        fprintf(fp, "\nTotal rivers found: %d\n", count);
        fclose(fp);
        std::cout << "\nResults saved to: " << outFile << std::endl;
    }

    return 0;
}
