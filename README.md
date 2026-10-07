# water_quality253

`water_quality253` 是一个可嵌入调度系统的 C++17 整数最小费用流库。它在有向管段网络中确定各管段调水量，使每个节点满足“流出减流入等于供需量 `b`”，并最小化总费用。在此之上，库还提供单一保守物质的稳态混合评估，让调度人员了解不同水源混合后各用水点的水质。

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

- `invalid_input`：JSON 或模型字段非法，例如空/重复 id、未知端点、自环、非整数、越界、上下限矛盾、总供需不为零；不返回任何部分计划。超出 double 范围的数值（如 `1e400`）同样按非法输入处理，不会导致进程异常退出。
- `infeasible`：输入合法但不存在可行流量。输出仅由原始节点组成的非空集合 `node_set`，以及：
  - `supply_demand_sum`：集合 S 的供需和。
  - `outgoing_capacity_sum`：流出 S 的管段容量上限和。
  - `incoming_lower_sum`：流入 S 的管段流量下限和。

这些数字严格满足：

```text
supply_demand_sum > outgoing_capacity_sum - incoming_lower_sum
```

该割不等式直接证明不存在满足供需和上下限的方案；证书中不会混入算法辅助节点。

## 稳态水质混合评估

在原管网输入中追加 `sources` 数组，为每个 `b > 0` 的供水节点给出外来水浓度（mg/L）：

```json
{
  "nodes": [{"id": "A", "b": 5}, {"id": "B", "b": -5}],
  "edges": [{"id": "AB", "from": "A", "to": "B", "lower": 0, "upper": 10, "cost": 1}],
  "sources": [{"node": "A", "concentration_mg_l": 12.5}]
}
```

- `sources` 必须恰好覆盖所有 `b > 0` 的节点，不得重复，不得包含未知或非供水节点；浓度须为 0 至 1000000 的有限数，单位 mg/L。
- 水量仍沿用原最小费用流求解的同一最优方案（立方米），不会另选“更清洁”的同价计划。
- 非法输入或无解时沿用原状态与证据输出，不含水质部分。

### 混合假设

- 节点无存储，外来供水与全部入流瞬时完全混合，用水和各出流取相同浓度。
- 供水节点也可能有管段入流，其混合浓度由质量守恒联立求解，不直接固定为外来水浓度。
- 物质沿管段无衰减、无生成，不考虑输送延迟；仅正流量管段传递物质。
- 支持分流、合流、平行管段和循环。有外来水源可达的循环按联立质量守恒求稳态浓度；无外来水源的闭合循环浓度不确定，输出 `null`（不擅自填零）；没有水经过的节点标记 `has_water: false`，浓度为 `null`。

### 水质输出

成功时在原输出上追加 `quality` 字段：

- `quality.nodes`：按输入节点顺序给出 `has_water` 与 `concentration_mg_l`（不确定或无水为 `null`）；用水节点（`b < 0`）另含 `removed_mass_g`，即取走的物质量（克；1 mg/L × 1 m³ = 1 g，故数值等于浓度乘以用水量）。
- `quality.edges`：按输入管段顺序给出出水浓度 `concentration_mg_l`，零流量或上游不确定时为 `null`。
- `quality.tolerance_mg_l`：混合求解使用的数值容差（mg/L），用于判断浓度是否可视为相等。

```sh
make quality-example
./build/water_quality253 < examples/quality_example.json
```

命令行程序根据输入是否含 `sources` 自动选择水量或水质模式，退出码不变。

## 构建与示例

要求 g++ 11.4、GNU Make 4.3 或兼容版本。

```sh
make all
make test
make example
make quality-example
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

水质评估：

```cpp
#include "water_quality253/water_quality.h"

water_quality253::QualityModel quality_model;
quality_model.network = model;  // 原水量模型
quality_model.sources = {{"A", 12.5}};

water_quality253::QualityResult quality = water_quality253::solve_quality(quality_model);
// 或使用 JSON：solve_quality_json_string / quality_result_to_json
```

调用之间无共享状态，求解过程在内部副本和残量网络中进行，不会修改输入对象。

## 实现概要

- `src/validation.cpp`：执行模型级严格校验并建立节点索引；同时校验外来水浓度清单与供水节点一一对应。
- `src/residual_network.cpp`：维护前向/反向弧及残量容量。
- `src/min_cost_flow.cpp`：固定下界、构造超源/超汇，使用带势 Dijkstra 连续最短路求整数最小费用流。
- `src/solution_builder.cpp`：恢复原管段流量、节点净流出，或从残量可达集恢复原始节点割证书。
- `src/water_quality.cpp`：在正流量图上做强连通分量分解，对有源分量按拓扑序联立质量守恒（高斯消元）求稳态浓度；无源闭合循环及其下游标记为不确定，无水节点单独标记。
- `src/json_adapter.cpp`：复用核心库接口完成 JSON 输入输出适配。
