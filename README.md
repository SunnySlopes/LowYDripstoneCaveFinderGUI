# Low Y Dripstone Cave & River Finder For Drowned Farm

A GUI tool for searching low-Y dripstone cave and river biomes in Minecraft Java Edition 1.18+.

用于在 Minecraft Java 版 1.18+ 中搜索低 Y 高度（滴水石洞穴 + 河流）联合区域的 GUI 工具。

## How to Run / 如何运行

**Java 17 or higher is required.**

**此程序需要 Java 17 或更高版本。**

Right-click the `.jar` file, choose **Open with**, and select **Java 17+** to launch the program.

右键 `.jar` 文件，选择**打开方式**，并使用 **Java 17 及以上版本**启动程序。

## How to Use / 如何使用

The program has two tabs: **Single Seed Search** and **Search from Seed List**.

程序有两个标签页：**单种子搜索**和**从种子列表搜索**。

Both tabs include a **Fast Mode** checkbox (on by default). See [Fast Mode / Precise Mode](#fast-mode--precise-mode--快速模式与精确模式) below.

两个标签页均有**快速模式**勾选框（默认勾选）。详见下方[快速模式与精确模式](#fast-mode--precise-mode--快速模式与精确模式)。

### Single Seed Search / 单种子搜索

- Enter seed, thread count, minimum weighted total area, minimum weighted total ratio, Fast Mode, and search range (`MinX/MaxX/MinZ/MaxZ`).
- Click **Start Search** to begin. You can **Pause**, **Resume**, or **Stop**.
- Results show coordinates, weighted total (`s=`), ratio, and reference `cave`/`river` values. You can **Sort** and **Export**.
- Use **Reset Search Area to Default** to restore default range.

- 输入种子、线程数、最低加权总面积、最低加权总面积占比、快速模式和搜索范围（`MinX/MaxX/MinZ/MaxZ`）。
- 点击**开始搜索**开始任务，可在过程中**暂停**、**继续**或**停止**。
- 结果显示坐标、加权总面积（`s=`）、占比及参考 `cave`/`river` 值，可进行**排序**和**导出**。
- 使用**重置搜索区域为默认值**恢复默认范围。

### Search from Seed List / 从种子列表搜索

- Click **Select File** and choose a text file with one seed per line.
- Configure parameters (including Fast Mode), then click **Start Search** for batch search.
- After completion, you can export full results or export only seed list.
- Supports sorting by area or distance.

- 点击**选择文件**，选择每行一个种子的文本文件。
- 设置参数（含快速模式）后点击**开始搜索**进行批量搜索。
- 完成后可导出完整结果或仅导出种子列表。
- 支持按面积或按距离排序。

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

- Native code samples biomes at fixed heights **Y = -60, -56, -52** (cave + river averaged).
- **Weighted total** (`total` / `s=` in output): `cave + river × river weight factor` (UI default 0.7)
- Sorting, filtering threshold, and percentage use **total**; `cave` and `river` are reference display only.
- Result line format: `/tp x 64 z s=total/49662 = pct% cave=N river=M`
- Weighted total ratio range: Fast Mode **60%～95%**, Precise Mode **40%～95%**.

- Native 在固定高度 **Y = -60、-56、-52** 采样群系（洞穴+河流取平均）。
- **加权总面积**（输出中的 `total` / `s=`）：`cave + river × 河流折算系数`（界面默认 0.7）
- 排序、阈值过滤、占比均基于 **total**；`cave` 与 `river` 仅作参考显示。
- 结果行格式：`/tp x 64 z s=total/49662 = pct% cave=N river=M`
- 加权总面积占比范围：快速模式 **60%～95%**，精确模式 **40%～95%**。

## Features / 功能特点

- Multi-threaded low-Y cave and river search
- Pause, resume, and stop controls
- Real-time progress and time estimation
- Batch search from seed list files
- Result sorting and export support

- 多线程低 Y 洞穴+河流搜索
- 支持暂停、继续和停止
- 实时进度与时间估算
- 支持从种子列表批量搜索
- 支持结果排序与导出

## Build / 构建

This project uses **prebuilt native libraries only** (no CMake in Gradle).

本项目**仅使用预编译 native 库**（Gradle 不触发 CMake 编译）。

1. Build `libDripstoneCaveFinderLibJ.dll` in the `jni/` native tree (e.g. `jni/build-jni.bat`).
2. Copy the DLL to `native/windows/libDripstoneCaveFinderLibJ.dll`.
3. Build the JAR:

```bat
gradle buildMainJar
```

The DLL is packaged into the JAR automatically via `prepareNativeResources`.

DLL 由 `prepareNativeResources` 自动打入 JAR。

Run:

```bat
java -jar build\libs\LowYDripstoneCaveFinder-1.0.0.jar
```

Or use `run.bat`.

## Core Libraries / 核心依赖

- [RiverFinder (melationin)](https://github.com/melationin/riverfinder) - Reference implementation
- [cubiomes (xpple)](https://github.com/xpple/cubiomes) - via `jni/cubiomes` submodule (C sources)

- [RiverFinder (melationin)](https://github.com/melationin/riverfinder) - 参考实现
- [cubiomes (xpple)](https://github.com/xpple/cubiomes) - 通过 `jni/cubiomes` 子模块引入（C 源码）

## Notes / 注意事项

- Use a thread count that matches your CPU capability for best stability.
- Native libraries (`dll`/`so`) are packaged in the jar and extracted to a temporary directory at runtime.
- To update the native library, rebuild in `jni/` and overwrite `native/windows/libDripstoneCaveFinderLibJ.dll`.
- 建议根据 CPU 性能设置线程数以获得更稳定的体验。
- 原生库（`dll`/`so`）已打包进 jar，运行时会自动解压到临时目录加载。
- 更新 native 库时，在 `jni/` 重新编译并覆盖 `native/windows/libDripstoneCaveFinderLibJ.dll`。

---

**Find more efficient low-Y drowned farm, faster.**
**更快地找到更优质的低 Y 溺尸农场。**
