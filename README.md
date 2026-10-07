# water_quality253

`water_quality253` 是一个可嵌入调度系统的 C++17 整数最小费用流库。它在有向管段网络中确定各管段调水量，使每个节点满足“流出减流入等于供需量 `b`”，并最小化总费用。

本库不包含前端、压力模拟或设备控制，也不依赖成熟网络流求解库。JSON 解析使用随仓库提供的 `third_party/nlohmann/json.hpp`。

## 输入

```json
{
  "nodes": [
    {"id": "A", "b": 10},
    {"id": "B", "b": -10}
  ],
  "edges": [
    {"id": "AB", "from": "A", "to": "B", "lower": 2, "upper": 10, "cost": 5}
  ]
}
```

- 正数 `b` 表示供水，负数表示用水，0 表示中转节点。
- `lower <= flow <= upper`，流量、供需和容量均为整数。
- 允许平行管段和方向相反的管段，不允许自环。
- 支持 2 至 40 个节点、最多 120 条管段。
- `b`、`lower`、`upper` 的绝对值不超过 10000；`cost` 为 0 至 10000。
- 节点和管段 `id` 必须为非空字符串且各自唯一；总供需必须为零。

## 成功输出

- `flows`：严格按输入 `edges` 顺序给出每条管段的整数流量。
- `balances`：按输入节点顺序给出各节点净流出，即流出减流入。
- `total_cost`：64 位整数总费用，单位为分；`Σ flow × cost`。
- `status` 为 `success`，`certificate` 为 `null`。

## 非法输入与无解

两类问题分开表达：

- `invalid_input`：JSON 或模型字段非法，例如空/重复 id、未知端点、自环、非整数、越界、上下限矛盾、总供需不为零；不返回任何部分计划。
- `infeasible`：输入合法但不存在可行流量。输出仅由原始节点组成的非空集合 `node_set`，以及：
  - `supply_demand_sum`：集合 S 的供需和。
  - `outgoing_capacity_sum`：流出 S 的管段容量上限和。
  - `incoming_lower_sum`：流入 S 的管段流量下限和。

这些数字严格满足：

```text
supply_demand_sum > outgoing_capacity_sum - incoming_lower_sum
```

该割不等式直接证明不存在满足供需和上下限的方案；证书中不会混入算法辅助节点。

## 构建与示例

要求 g++ 11.4、GNU Make 4.3 或兼容版本。

```sh
make all
make test
make example
```

命令行示例从标准输入读取 JSON：

```sh
./build/water_quality253 < examples/example.json
```

成功退出码为 0，非法输入为 2，合法但无解为 3。

## C++ 调用

使用结构化模型：

```cpp
#include "water_quality253/water_flow.h"

water_quality253::Model model;
model.nodes = {{"A", 10}, {"B", -10}};
model.edges = {{"AB", "A", "B", 2, 10, 5}};

water_quality253::Result result = water_quality253::solve(model);
if (result.status == water_quality253::Status::Success) {
    auto cost = result.total_cost;
}
```

使用 JSON：

```cpp
#include "water_quality253/json_adapter.h"

auto result = water_quality253::solve_json_string(input_text);
nlohmann::json output = water_quality253::result_to_json(result);
```

调用之间无共享状态，求解过程在内部副本和残量网络中进行，不会修改输入对象。

## 稳态水质混合评估

在原有水量求解之上，库提供单一保守物质的稳态混合评估：给定每个供水节点（`b > 0`）外来水的浓度（mg/L），报告混合后各用水点的水质。

### 假设

- 水量仍沿用原最小费用流最优方案（立方米），不另选更清洁的同价计划。
- 节点无存储；外来供水与全部入流瞬时完全混合，该节点的用水和全部出流取相同浓度。
- 供水节点也可能有管段入流，其浓度同样由混合方程联立求得，不直接固定为外来浓度。
- 物质沿管段无衰减、无生成，不考虑输送延迟；仅正流量管段传递物质。
- 有外来水源可达的循环按联立质量守恒求稳态；无外来水源的闭合循环浓度不确定（`null`），不填零；没有任何水经过的节点标记为 `no_water`。
- 浓度须为 0 至 1000000 的有限数（mg/L）；每个 `b > 0` 的节点必须恰好出现一次，其他或未知 id 一律拒绝。
- 数值容差：线性系统采用部分主元高斯消元，浓度相对误差不超过约 `1e-9`（输出中的 `tolerance` 字段）。

### JSON 输入与输出

在原输入基础上增加 `source_quality` 数组：

```json
{
  "nodes": [{"id": "S", "b": 5}, {"id": "D", "b": -5}],
  "edges": [{"id": "sd", "from": "S", "to": "D", "lower": 0, "upper": 10, "cost": 1}],
  "source_quality": [{"id": "S", "concentration_mg_l": 42.5}]
}
```

成功输出在原水量字段（`flows`、`balances`、`total_cost`）之外包含：

- `tolerance`：数值容差（相对，约 `1e-9`）。
- `node_quality`：按输入节点顺序给出 `id`、`state`（`ok` / `undetermined` / `no_water`）与 `concentration_mg_l`；不确定或无水时为 `null`。用水节点（`b < 0`）另含 `withdrawn_g`，即取走的物质量（克，`mg/L × m³ = g`）。
- `edge_quality`：按输入管段顺序给出出水浓度（即上游节点混合浓度）；零流量或浓度不确定的管段为 `null`。

非法输入（`invalid_input`）或无解（`infeasible`）时沿用原状态与割证书，不交付任何水质结果。JSON 数值溢出（如 `1e400`）同样按 `invalid_input` 处理，不会异常退出。

### 命令行与 C++ 调用

```sh
make all
make quality-example
./build/water_quality253_quality < examples/quality_example.json
```

```cpp
#include "water_quality253/water_quality.h"

water_quality253::Model model;
std::vector<water_quality253::SourceQualityInput> sources = {{"S", 42.5}};
water_quality253::QualityResult result =
    water_quality253::solve_quality(model, sources);
```

JSON 适配：`solve_quality_json_string` / `quality_result_to_json`，见 `include/water_quality253/json_adapter.h`。

## 实现概要

- `src/validation.cpp`：执行模型级严格校验并建立节点索引。
- `src/residual_network.cpp`：维护前向/反向弧及残量容量。
- `src/min_cost_flow.cpp`：固定下界、构造超源/超汇，使用带势 Dijkstra 连续最短路求整数最小费用流。
- `src/solution_builder.cpp`：恢复原管段流量、节点净流出，或从残量可达集恢复原始节点割证书。
- `src/water_quality.cpp`：校验外来水浓度输入并复用原求解取得完整最优方案。
- `src/quality_mixing.cpp`：按正流量拓扑联立稳态质量守恒，标记不确定循环与无水节点。
- `src/json_adapter.cpp`：复用核心库接口完成水量与水质 JSON 输入输出适配。
