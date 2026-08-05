#ifndef SEARCH_CONFIG_H
#define SEARCH_CONFIG_H

namespace SearchConfig {

/** Phase1a 全图粗滤模式；由 PHASE1_COARSE_FILTER 选定。 */
enum class Phase1CoarseFilter {
    WeirdnessOnly = 0,
    ContinentalnessOnly = 1,
    ContThenWeird = 2,
    WeirdThenCont = 3,
};

/** Phase1a 粗滤：大陆性已由 CONT 预筛覆盖，掩膜内只采 Weirdness。 */
constexpr Phase1CoarseFilter PHASE1_COARSE_FILTER = Phase1CoarseFilter::WeirdnessOnly;

/**
 * Phase1 预筛：先按此步长采大陆性，超过 CONT_PREFILTER_THRESHOLD 的 tile
 * 再膨胀 CONT_PREFILTER_DILATE 圈，之后只在掩膜内做 PHASE1_WEIRDNESS_GRID_SCALE 扫描。
 */
constexpr int PHASE1_CONT_PREFILTER_SCALE = 64;
constexpr double PHASE1_CONT_PREFILTER_THRESHOLD = 0.525;
constexpr int PHASE1_CONT_PREFILTER_DILATE = 1;

/** Phase1 环状扫描网格步长（方块），Phase1a 粗滤（预筛之后）。 */
constexpr int PHASE1_WEIRDNESS_GRID_SCALE = 16;

/** Phase1 气候精筛网格步长（方块）。 */
constexpr int PHASE1_CLIMATE_GRID_SCALE = 4;

/** Phase2 精确群系采样网格步长（方块）。 */
constexpr int PRECISE_GRID_SCALE = 1;

/** Phase1 候选块聚合边长（方块）。 */
constexpr int CANDIDATE_TILE_BLOCKS = 256;

/** 气候精筛子区域半径（方块）。 */
constexpr int SUBSEARCH_RADIUS_BLOCKS = 256;
constexpr int SUBSEARCH_SIZE_BLOCKS = SUBSEARCH_RADIUS_BLOCKS * 2;

/** Phase2 精采样窗口（方块）。 */
constexpr int REFINE_WINDOW_BLOCKS = 320;
constexpr int REFINE_HALF_WINDOW = REFINE_WINDOW_BLOCKS / 2;

static_assert(PHASE1_CONT_PREFILTER_SCALE % PHASE1_WEIRDNESS_GRID_SCALE == 0,
              "PHASE1_CONT_PREFILTER_SCALE must be a multiple of PHASE1_WEIRDNESS_GRID_SCALE");
static_assert(PHASE1_WEIRDNESS_GRID_SCALE % PHASE1_CLIMATE_GRID_SCALE == 0,
              "PHASE1_WEIRDNESS_GRID_SCALE must be a multiple of PHASE1_CLIMATE_GRID_SCALE");

} // namespace SearchConfig

#endif
