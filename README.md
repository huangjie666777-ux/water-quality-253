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

## 实现概要

- `src/validation.cpp`：执行模型级严格校验并建立节点索引。
- `src/residual_network.cpp`：维护前向/反向弧及残量容量。
- `src/min_cost_flow.cpp`：固定下界、构造超源/超汇，使用带势 Dijkstra 连续最短路求整数最小费用流。
- `src/solution_builder.cpp`：恢复原管段流量、节点净流出，或从残量可达集恢复原始节点割证书。
- `src/json_adapter.cpp`：复用核心库接口完成 JSON 输入输出适配。
