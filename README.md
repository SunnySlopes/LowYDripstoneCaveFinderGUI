# Low Y Dripstone Cave & River Finder For Drowned Farm

A GUI tool for searching low-Y dripstone cave and river biomes in Minecraft Java Edition 1.18+.

用于在 Minecraft Java 版 1.18+ 中搜索低 Y 高度（滴水石洞穴 + 河流）联合区域的 GUI 工具。

## How to Run / 如何运行

**Java 17 or higher is required.**

**此程序需要 Java 17 或更高版本。**

Right-click the `.jar` file, choose **Open with**, and select **Java 17+** to launch the program.

右键 `.jar` 文件，选择**打开方式**，并使用 **Java 17 及以上版本**启动程序。

## How to Use / 如何使用

The program has three tabs: **Single Seed Search**, **Search from Seed List**, and **Fast Single Seed Search**.

程序有三个标签页：**单种子搜索**、**从种子列表搜索**和**快速单种子搜索**。

The first two tabs include a **Fast Mode** checkbox (on by default) and a **Game Version** selector: `1.18~26.1` (cubiomes 26.1) or `26.2+` (cubiomes 26.3, includes sulfur caves). See [Fast Mode / Precise Mode](#fast-mode--precise-mode--快速模式与精确模式) below. The third tab is a separate large-pit search; see [Fast Single Seed Search](#fast-single-seed-search--快速单种子搜索).

前两个标签页均有**快速模式**勾选框（默认勾选）和**游戏版本**选项：`1.18~26.1`（按 cubiomes 26.1）或 `26.2+`（按 cubiomes 26.3，含硫磺洞穴）。详见下方[快速模式与精确模式](#fast-mode--precise-mode--快速模式与精确模式)。第三个标签页是另一条大坑搜索，详见[快速单种子搜索](#fast-single-seed-search--快速单种子搜索)。

### Single Seed Search / 单种子搜索

- Enter seed, thread count, minimum weighted total area, minimum weighted total ratio, river weight, sample Y range (`Y Max` / `Y Min`), Fast Mode, and search range (`MinX/MaxX/MinZ/MaxZ`).
- The sample Y range defaults to **-63…-51** (13 layers). `Y Max - Y Min + 1` must not exceed **48**; inverted, out-of-bounds (`-63…319`) or too wide ranges are rejected with an error, and the fields turn red while the input is invalid.
- Two non-blocking warnings appear after clicking **Start Search**: span over 24 layers, and a lower bound above -58. Both continue the search when acknowledged.
- Click **Start Search** to begin. You can **Pause**, **Resume**, or **Stop**.
- Results show coordinates, weighted total (`s=`), ratio, and reference `cave`/`river` values. You can **Sort** and **Export**.
- Use **Reset Search Area to Default** to restore default range.

- 输入种子、线程数、最低加权总面积、最低加权总面积占比、河流折算系数、采样 Y 范围（`Y上限`/`Y下限`）、快速模式和搜索范围（`MinX/MaxX/MinZ/MaxZ`）。
- 采样 Y 范围默认 **-63…-51**（13 层）。`Y上限 - Y下限 + 1` 不得超过 **48** 层；倒置、越界（`-63…319`）或跨度过大都会直接报错，输入非法时输入框会实时标红。
- 点击**开始搜索**后会弹两条不阻断流程的提示：跨度超过 24 层、最低层高于 -58。
- 点击**开始搜索**开始任务，可在过程中**暂停**、**继续**或**停止**。
- 结果显示坐标、加权总面积（`s=`）、占比及参考 `cave`/`river` 值，可进行**排序**和**导出**。
- 使用**重置搜索区域为默认值**恢复默认范围。

### Search from Seed List / 从种子列表搜索

- Click **Select File** and choose a text file with one seed per line.
- Configure parameters (including the sample Y range and Fast Mode), then click **Start Search** for batch search. Every seed uses the same Y range.
- After completion, you can export full results or export only seed list.
- Supports sorting by area or distance.

- 点击**选择文件**，选择每行一个种子的文本文件。
- 设置参数（含采样 Y 范围与快速模式）后点击**开始搜索**进行批量搜索；同一批种子共用同一个 Y 范围。
- 完成后可导出完整结果或仅导出种子列表。
- 支持按面积或按距离排序。

### Fast Single Seed Search / 快速单种子搜索

Use this tab when you trust the seed (its weirdness y-offset is low enough(<0.75, only 20% of all seeds meet the demand), which means more variated weirdness and more pits). A hit can show up in about **1% to 5%** of the time a normal search would take. The longer it takes, the more likely the best one is in it. If it comes back empty, run the normal single-seed search.

Coordinates default to the world border (±30000000) and can be changed. Like the other tabs, a **square side length** checkbox fills the four bounds from a square centered at (0, 0) and locks those fields. **Reset Coordinates to World Border** checks the box again and restores that side length.

- Weighted area ratio is **80%–95%**, default **90%**. There is no river-weight field. River area is not counted, and result lines omit `cave` / `river`: `/tp x -40 z s=total/49659 = pct%`.
- This tab has the same **sample Y range** fields (`Y Max` / `Y Min`, default -63…-51, span cap 48). The large-pit precise pass uses the same 24–128 shell model (AFK Y = -40), so the score is the average cave content inside that shell over the configured window (still 0..49659, no river contribution).
- This tab does not use the Fast Mode checkbox. Progress is four phases: octave cache, finding large pits, continentalness check, precise check of candidate points.
- Click **Start Search** yourself. Switching to this tab never starts a search.

对种子够自信（奇异值 y-offset 足够低（<0.75，只有约20%种子满足条件），通常意味着奇异值波动大，全图有更多“大坑”）时先用这一页。出结果大约只要正常搜索 **1%～5%** 的时间，耗时越长说明最大值越有可能就在其中。空手而归就回到正常的单种子搜索。

坐标默认是世界边界（±30000000），可以改。和另外两页一样有**正方形区域边长**：勾选后以 (0, 0) 为中心填入四个坐标并锁住它们。**重置坐标为世界边界**会重新勾选，并把边长恢复成世界边界。

- 加权总面积占比 **80%～95%**，默认 **90%**。没有河流折算系数（因为这种搜索结果附近肯定没有河流），不计入河流面积，结果行也不带 `cave` / `river`：`/tp x -40 z s=total/49659 = pct%`。
- 这一页同样有**采样 Y 范围**填框（`Y上限` / `Y下限`，默认 -63…-51，跨度上限 48）。大坑精算走同一条 24–128 球壳规则（挂机 Y = -40），计分为该球壳在配置窗口内的逐层平均（仍是 0..49659，不计河流）。
- 这一页不用快速模式勾选框。进度分四段：噪声缓存、寻找大坑、大陆性检查、精确检查潜在点位。
- 需要自己点**开始搜索**。切到这一页不会自动开搜。

#### Jump from Single Seed Search / 从单种子搜索跳转

The prompt appears only when you click **Start Search** on **Single Seed Search**, and only when the search box is at least **6000001 × 6000001** blocks. The program then scores the seed's weirdness y-offset. Thresholds live in `WeirdnessYOffset`: `MIN_SEARCH_AREA`, `LUCKY_BELOW` (0.75), `LUCKY_LOW_PROB_BELOW` (0.85).

| Where / 位置 | y-offset | What happens / 行为 |
|---|---|---|
| First tab / 第一页 | `< 0.75` | Offers fast search / 询问要不要去快速搜索 |
| First tab / 第一页 | `≥ 0.75` | No dialog. The original search starts / 不弹框，直接开始原搜索 |
| Fast tab, before search / 快速页，开搜前 | `> 0.85` | Warns that fast search will likely come back empty / 提示快速搜索很可能会空手而归 |
| Fast tab, before search / 快速页，开搜前 | `≤ 0.85` | Search starts / 直接开始搜索 |

The seed-list tab never shows these dialogs. Closing a dialog starts neither search.

- **去碰碰运气! / have a try!** switches to **Fast Single Seed Search** and copies the seed, thread count, game version, coordinates, and the sample Y range. If square-side mode is on, the side length is copied and the box stays checked. The area ratio stays at this tab's **90%** default. River weight stays on the first tab. The search waits until you click **Start Search**.
- **算了吧，继续原搜索 / don't cheat me! remain on this page** continues the search you just started.
- On the fast tab, **I'll try anyway / 我偏要试试** starts the fast search. **OK / 好的** switches back to **Single Seed Search** and copies the seed, thread count, game version, coordinates, area ratio, and the sample Y range. River weight and Fast Mode stay at the values already on the first tab (a blank river weight is filled with the default 0.70). That jump does not start a search.

只有在**单种子搜索**里点**开始搜索**，并且搜索范围不小于 **6000001 × 6000001** 格时，才会询问要不要去快速搜索。程序计算该种子的奇异值 y-offset。阈值在 `WeirdnessYOffset`：`MIN_SEARCH_AREA`、`LUCKY_BELOW`（0.75）、`LUCKY_LOW_PROB_BELOW`（0.85）。offset 不低于 0.75 时，第一页不再弹框。

种子列表页不会弹出这些框。关掉弹框则两边都不开始搜索。

- **去碰碰运气! / have a try!** 切到**快速单种子搜索**，并带上种子、线程数、游戏版本、坐标和采样 Y 范围。第一页开着正方形模式时，边长一并带上并保持勾选。面积占比保持本页默认 **90%**。河流折算系数留在第一页。需要再点一次**开始搜索**才会执行。
- **算了吧，继续原搜索 / don't cheat me! remain on this page** 继续刚才那次原搜索。
- 快速页上，**我偏要试试**会照常开始快速搜索。**好的**跳回**单种子搜索**，带上种子、线程数、游戏版本、坐标、面积占比和采样 Y 范围。河流折算系数和快速模式保留第一页上已有的值（折算系数为空时填默认 0.70）。这次跳转不会自动开始搜索。

## Fast Mode / Precise Mode / 快速模式与精确模式

Phase 1 coarse scan uses continentalness (Cont) + weirdness (ridge) climate sampling at different grid scales:

阶段 1 粗筛使用大陆性（Cont）+ 山脊/怪异度（Weirdness）气候噪声，采样步长不同：

| Mode / 模式 | Cont scale | Weirdness scale | Min weighted ratio / 面积占比下限 | Trade-off / 取舍 |
|---|---|---|---|---|
| **Fast Mode** (default, checked) / **快速模式**（默认勾选） | 128 | 32 | 60% | Much faster; may miss a few lower-area hits / 速度大幅提升，可能漏掉少量面积偏低的结果 |
| **Precise Mode** (unchecked) / **精确模式**（取消勾选） | 32 | 16 | 40% | Slower, fewer misses; min ratio can go down to 40% / 更慢、漏检更少，面积下限可至 40% |

- Fast Mode is recommended for large ranges or seed-list batch jobs.
- Uncheck Fast Mode when you need more complete coverage of lower-area candidates (down to 40%).
- Upper bound of the weighted ratio remains **95%** in both modes.

- 大范围或种子列表批量搜索建议使用快速模式。
- 需要更完整覆盖面积偏低候选点（下限至 40%）时，取消勾选快速模式。
- 两种模式的加权总面积占比上限均为 **95%**。

## Search Semantics / 搜索语义

- The native code samples **every layer in `[Y Min, Y Max]`** and scores the **24–128 spherical shell around (result x, -40, result z)**: each layer counts every cell inside that layer's shell cross-section, and the score is the per-layer average (cave + river averaged). The default window is **-63…-51** (13 layers); the span is capped at **48** layers.
- AFK Y is fixed at **-40**. The shell is centered on `(result x, -40, result z)`; every cell inside the shell counts. The CLI can override it with `--afk-y` / `--sweep-afk-y`. Because the shell cross-section is close to the full horizontal ring, the theoretical maximum ratio is near 100%.
- **Weighted total** (`total` / `s=` in output): `cave + river × river weight factor` (UI default **0.70**; the base CLI default stays 0.75)
- Sorting, filtering threshold, and percentage use **total**; `cave` and `river` are reference display only.
- Result line format: `/tp x -40 z s=total/49659 = pct% cave=N river=M`
- Weighted total ratio range: Fast Mode **60%～95%**, Precise Mode **40%～95%**. Default is **70%** (`DEFAULT_RIVER_AREA_UI`).
- Result-set model tag: `drowned-weighted-shell-b/{L}/afk-40` (e.g. `/13/afk-40`). Exports and the CLI report carry it; results from different layer counts, AFK Y or model versions are **not comparable**.
- The third tab (Fast Single Seed Search / large-pit) has the same sample Y range fields and uses the same 24–128 shell average over the configured window; it does not count river area.

- Native 逐层采样 **`[Y下限, Y上限]` 的每一层**，统计以**结果点 (x, -40, z)** 为球心的 **24–128 球壳**：每层只计该层球壳截面内的格，最终取逐层平均（洞穴+河流平均）。默认窗口 **-63…-51**（13 层），跨度硬上限 **48** 层。
- 挂机 Y 固定为 **-40**。球壳以 `(结果x, -40, 结果z)` 为球心，球壳内所有格都计入；CLI 可用 `--afk-y` / `--sweep-afk-y` 覆盖。由于球壳截面接近完整水平环，理论最高占比接近 100%。
- **加权总面积**（输出中的 `total` / `s=`）：`cave + river × 河流折算系数`（界面默认 **0.70**；base CLI 默认仍为 0.75）
- 排序、阈值过滤、占比均基于 **total**；`cave` 与 `river` 仅作参考显示。
- 结果行格式：`/tp x -40 z s=total/49659 = pct% cave=N river=M`
- 加权总面积占比范围：快速模式 **60%～95%**，精确模式 **40%～95%**。默认 **70%**（`DEFAULT_RIVER_AREA_UI`）。
- 结果模型标识：`drowned-weighted-shell-b/{L}/afk-40`（如 `/13/afk-40`）。导出与 CLI 报告都会带上；不同层数、挂机 Y 或模型版本的结果**不可比**。
- 第三个标签页（快速单种子搜索 / 大坑）也有同样的采样 Y 范围填框，并按配置窗口走同一条 24–128 球壳规则（逐层平均），不计河流面积。

### Threshold calibration / 门槛标定

The shell model changes the score relative to the old horizontal cylinder: cells outside the 24–128 sphere are excluded per layer, so a point's value can move by a few percent up or down. The product default stays **70%** (`DEFAULT_RIVER_AREA_UI`); re-run a few seeds and lower the ratio if you want a looser threshold. The base CLI default `--min-area` is 70% as well.

球壳模型相对旧的水平柱环会改变分数：每层都会排除 24–128 球体之外的格，同一点的分值可能上下浮动几个百分点。产品默认仍为 **70%**（`DEFAULT_RIVER_AREA_UI`）；如果希望门槛更宽松，可以多跑几个种子后适当降低占比。base CLI 的 `--min-area` 默认也是 70%。

## Features / 功能特点

- Adjustable precise sample Y window (`Y Min` / `Y Max`, default -63…-51, hard span cap 48) with live validation and two non-blocking warnings
- 24–128 spherical-shell scoring around the fixed AFK point Y=-40 over every sampled layer (per-layer average units)
- Multi-threaded low-Y cave and river search
- Pause, resume, and stop controls
- Real-time progress and time estimation
- Batch search from seed list files
- Result sorting and export support
- Frozen-original reference regression tests (2808 block biomes) and analytic shell-geometry tests

- 可调精采样 Y 范围（`Y下限`/`Y上限`，默认 -63…-51，跨度硬上限 48），输入实时校验 + 两条非阻断提示
- 以固定挂机点 Y=-40 为球心的 24–128 球壳计分，逐层统计后取平均（逐层平均口径）
- 多线程低 Y 洞穴+河流搜索
- 支持暂停、继续和停止
- 实时进度与时间估算
- 支持从种子列表批量搜索
- 支持结果排序与导出
- 原版冻结参照回归（2808 格）与球壳几何解析用例

## Build / 构建

All C++/cubiomes computation lives in the
[LowYDripstoneCaveFinderBase](https://github.com/SunnySlopes/LowYDripstoneCaveFinderBase)
submodule. The main module only owns the Swing GUI, query orchestration and
the threshold / page-jump decisions.

全部 C++ / cubiomes 计算代码位于
[LowYDripstoneCaveFinderBase](https://github.com/SunnySlopes/LowYDripstoneCaveFinderBase)
子模块。主模块只保留 Swing 界面、查询编排以及阈值判断 / 页面跳转逻辑。

1. Initialize the submodule and its nested cubiomes checkout:

```bat
git submodule update --init --recursive
```

2. Build the base DLL, the main fat JAR and the standalone CLI in one step:

```bat
gradlew buildDistribution
```

`buildDistribution` runs the submodule checks and the JNI contract check, then
builds:

- `build\libs\LowYDripstoneCaveFinder-1.4.3.jar` — GUI fat JAR with the base DLL embedded
- `build\cli\lowydripstonecavefinder-cli.exe` — standalone CLI
- `LowYDripstoneCaveFinderBase\build\native\libDripstoneCaveFinderLibJ.dll` — JNI library

The DLL is packaged into the JAR automatically via `prepareNativeResources`.

DLL 由 `prepareNativeResources` 自动打入 JAR。

Run the GUI:

```bat
java -jar build\libs\LowYDripstoneCaveFinder-1.4.3.jar
```

Or use `run.bat`. The standalone CLI can be run directly:

```bat
build\cli\lowydripstonecavefinder-cli.exe --seed 12345 --out result.txt
build\cli\lowydripstonecavefinder-cli.exe --seed 12345 --square 131072 --y-min -60 --y-max -52 --out y9.txt
build\cli\lowydripstonecavefinder-cli.exe --weirdness-y-offset 12345
```

Native regression tests (shell geometry, layer-window validation, frozen
2808-cell biome reference ported from the C project):

```bat
powershell -ExecutionPolicy Bypass -File LowYDripstoneCaveFinderBase\tests\run_tests_msys2.ps1
```

The base submodule can also be built independently; see
`LowYDripstoneCaveFinderBase/README.md`.

## Core Libraries / 核心依赖

- [LowYDripstoneCaveFinderBase](https://github.com/SunnySlopes/LowYDripstoneCaveFinderBase) - JNI engine + standalone CLI
- [cubiomes (xpple)](https://github.com/xpple/cubiomes) - nested submodule of `LowYDripstoneCaveFinderBase`
- [RiverFinder (melationin)](https://github.com/melationin/riverfinder) - Reference implementation

- [LowYDripstoneCaveFinderBase](https://github.com/SunnySlopes/LowYDripstoneCaveFinderBase) - JNI 引擎与独立 CLI
- [cubiomes (xpple)](https://github.com/xpple/cubiomes) - `LowYDripstoneCaveFinderBase` 的嵌套子模块
- [RiverFinder (melationin)](https://github.com/melationin/riverfinder) - 参考实现

## License / 许可证

This project is released under the **MIT License**; see [LICENSE](LICENSE).

The native search core in the `LowYDripstoneCaveFinderBase` submodule is MIT-licensed as well.
It builds on [cubiomes](https://github.com/Cubitect/cubiomes) by Cubitect (MIT).

本项目以 **MIT** 许可证发布，详见 [LICENSE](LICENSE)。

`LowYDripstoneCaveFinderBase` 子模块同样采用 MIT 许可证；其原生引擎基于 Cubitect 的
[cubiomes](https://github.com/Cubitect/cubiomes)（MIT）。

## Notes / 注意事项

- Use a thread count that matches your CPU capability for best stability.
- Native libraries (`dll`/`so`) are packaged in the jar and extracted to a temporary directory at runtime.
- The weirdness y-offset score is computed by the base submodule through
  `LowYDripstoneCaveFinderBridge.weirdnessYOffset(long)`; the `0.75` / `0.85`
  thresholds and the fast-search page jump stay in the GUI.
- To update the native code, change `LowYDripstoneCaveFinderBase` and rebuild
  with `gradlew buildDistribution`.
- The GUI passes `riverWeight = 0.7` on both search tabs; the base CLI default is
  still `0.75` (B3).
- Batch search reuses the native `Generator` across seeds (B6), so the per-seed
  `setupGenerator` cost (~0.29 s) is paid once per search session instead of once
  per seed.
- Exports start with a `# model=drowned-weighted-shell-b/{L}/afk-40` comment;
  results from different layer windows or AFK Y are not comparable.
- 建议根据 CPU 性能设置线程数以获得更稳定的体验。
- 原生库（`dll`/`so`）已打包进 jar，运行时会自动解压到临时目录加载。
- 奇异值 y-offset 分数由 base 子模块通过
  `LowYDripstoneCaveFinderBridge.weirdnessYOffset(long)` 计算；`0.75` / `0.85`
  阈值判断与快速搜索页跳转仍由主模块执行。
- 界面两个搜索页传 `riverWeight = 0.7`；base CLI 默认仍为 `0.75`（B3）。
- 批量搜索复用原生 `Generator`（B6），`setupGenerator` 的约 0.29 s 每个搜索会话
  只付一次，而不是每个种子一次。
- 导出文件开头带 `# model=drowned-weighted-shell-b/{L}/afk-40` 注释；不同层窗口或挂机 Y 的结果不可比。
- 更新 native 代码时修改 `LowYDripstoneCaveFinderBase`，然后执行
  `gradlew buildDistribution`。

---

**Find more efficient low-Y drowned farm, faster.**
**更快地找到更优质的低 Y 溺尸农场。**
