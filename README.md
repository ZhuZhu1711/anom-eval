# 水平坏点簇检测 A/B 测试工程

对照实现来自 `Find_ClusterType_in_binarymap_Type15`（二值坏点图、4 像素打包、OTP type 1–15）。实验组只改**水平方向查点**，用来补上「能被 DPC 放过的单点 / 相邻双点，一旦在组边界上同通道相连」时的漏检。

## 算法在做什么

输入是一张二值图：`255` 坏点，`0` 好点。宽度必须是 4 的倍数。每一行按 4 像素一组编码：

```
pos0 pos1 pos2 pos3  →  bit3 bit2 bit1 bit0
```

流程两边共用：

1. **大簇**：5×5 窗口 ≥16 个坏点，或同通道 4 连点（步长 2）→ 直接判大簇返回。
2. **组内簇**：一组里 2 个及以上坏点，且**没有水平相邻对** → 记入 `type_map`。水平相邻双坏点在这一步被 skip，这是故意的（DPC 可消）。
3. **水平查点（有 bug 的那一步 / 实验组修改点）**
4. **垂直单点**：保持原逻辑，两边相同。

### 设计约束（不要改错）

- 孤立水平单坏点：DPC 可消（优先用同通道左边的好点），**不应该**记簇。
- 孤立水平相邻双坏点：DPC 可消，**故意 skip**。
- 上面两类一旦在组边界上形成同通道、距离 2 的连接，右边坏点的 DPC 左源就是坏点，**必须**记下来。这包括和相邻簇拼接，也包括两个隔组相对的单点（例如 `..X.X...`）：几何上等于组内的 `0b1010`。对照的 `map_img[loca+4]==255` 能抓住左组，但右组仍常漏。

组边界上的同通道距离 2：

- 左组 pos2 ↔ 右组 pos0
- 左组 pos3 ↔ 右组 pos1

### 原水平查点的两个洞

1. 只处理 `sum==255`（组内恰好 1 个坏点）。相邻双点被 skip 后像素还在，但再也不会被拿去和旁边的簇组合。
2. 单点匹配表不完整。例如左邻簇是 `0b1001`、本组是 pos1 单点时，`type_0100` 里没有 `0b1001`，直接漏。

实验组用「和邻组是否同通道相连」一次扫完（邻组可以是已记录的组内簇、残留的 ≥2 坏点组、或另一个单点），并把**完整 mask** 写入 `type_map`（相邻双点不再被压成单 bit）。

## 两种测试

| 模式 | 数据 | 用途 |
|------|------|------|
| 固定覆盖率 | `testdata/manual_cases.txt` + `testdata/generated_coverage.txt` | 每次回归都跑同一批，文件可手改 |
| 随机稀疏 | 运行时生成，默认 8000 张 `192×96`、每张 24 点 | 高频次、看性能和偶发组合 |

覆盖率网格按原算法的 4 包切分穷举：

- 相邻两组 mask `0..15 × 0..15`，放在行首 / 行中 / 行尾
- 连续三组 `16³−1` 种非空组合（链：簇\|单\|相邻双 等）

## 构建与运行

依赖：C++17，CMake ≥ 3.16。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/dd_ab
```

常用参数：

```bash
# 只跑固定用例
./build/dd_ab --no-random

# 重新写出覆盖率网格（会覆盖 testdata/generated_coverage.txt）
./build/dd_ab --write-coverage testdata/generated_coverage.txt

# 高频稀疏
./build/dd_ab --random 20000 --width 256 --height 128 --defects 20 --seed 1

# 看单条
./build/dd_ab --dump single_pos1_plus_cluster_1001
```

报告写到 `reports/report.txt` 和 `reports/report.html`。

手工用例格式见 `testdata/manual_cases.txt` 头部。也可以追加：

```
CASE my_case
W 16
H 3
MAP
................
X..X.X..........
................
END
```

或紧凑行（生成文件用的格式）：

```
PAIR id 16 3 1 1 9 4
TRIPLE id 12 3 1 0 10 12 5
```

`PAIR/TRIPLE` 字段：`id width height row group0 mask...`，mask 为 0–15。

## 目录

- `src/algorithm_control.cpp` — 原水平查点（对照）
- `src/algorithm_experimental.cpp` — 修改后的水平查点（实验）
- `src/algorithm_common.cpp` — 大簇 / 组内簇 / 垂直（两边共用）
- `src/oracle.cpp` — 独立的“该不该记”规则，用来打分
- `testdata/` — 固定测试数据
