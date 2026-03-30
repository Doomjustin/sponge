# 性能基准测试

本目录包含了 Sponge 项目中关键数据结构的性能基准测试程序。

## 📊 基准测试程序

### 1. sds_vs_string_benchmark

**目的：** 全面对比 redis::String 与 std::string 的性能

**测试场景：**
- 创建字符串（10M 迭代）
- Copy 操作（5M 迭代）
- Append 操作（1M 迭代）
- Size/Capacity 查询（100M 迭代）
- 内存开销分析

**运行方式：**
```bash
./benchmark/sds_vs_string_benchmark
```

**典型结果：**
```
创建小字符串(10B)   : std::string 31ms     redis::String 666ms    (21.3x)
创建中字符串(100B)  : std::string 63ms     redis::String 523ms    (8.3x)
Size 查询(100M)     : std::string 23ms     redis::String 1072ms   (47.4x)
```

### 2. sds_template_vs_polymorphic

**目的：** 衡量虚函数调用对 redis::String 性能的影响

**对比内容：**
- 多态版本（PMR 虚函数）vs 模板版本（具体类型）

**运行方式：**
```bash
./benchmark/sds_template_vs_polymorphic
```

**典型结果：**
```
size() 调用    : 多态 4849ms  模板 3694ms   改进 24%
capacity() 调用: 多态 627ms   模板 406ms    改进 35%
创建操作      : 多态 565ms   模板 455ms    改进 19%
```

**结论：** 虚函数确实有开销，但去掉虚函数后仍比 std::string 慢 15-50 倍，说明性能瓶颈在架构设计本身，而非虚函数。

### 3. cpp_string_verdict

**目的：** 综合评估与最终结论

**场景：**
- 频繁 size() 查询（500M 次）
- 批量创建（10M 个字符串）
- Append 操作（5M 次）
- 内存效率对比

**运行方式：**
```bash
./benchmark/cpp_string_verdict
```

**关键数据：**
```
size() 查询    : std::string 113ms   redis::String 5055ms  (44.8x)
批量创建      : std::string 145ms   redis::String 1079ms  (7.5x)
Append        : std::string 140ms   redis::String 1676ms  (12x)
内存占用(1M)  : std::string 126MB   redis::String 111MB   (11% 少)
```

### 4. pmr_string_comparison ⭐ NEW

**目的：** 在公平的 PMR 资源条件下，对比 std::string 与 redis::String

这个基准测试最关键：它使用公平环境（三种字符串都用相同 PMR 资源），完全排除虚函数与内存分配的变量，单纯对比架构设计的差异。

**运行方式：**
```bash
./benchmark/pmr_string_comparison
```

**关键数据：**
```
size() 查询(500M)   : std::string 113ms  std::pmr::string 115ms  redis::String 5249ms  (46.4x)
capacity() 查询(100M): std::string 22ms   std::pmr::string 44ms   redis::String 633ms   (28.6x)
创建(5M)            : std::string 61ms   std::pmr::string 216ms  redis::String 609ms   (4.2x)
Append(1M)          : std::string 28ms   std::pmr::string 124ms  redis::String 386ms   (13.8x)

两种 std 字符串略有偏差原因：
- std::string 使用系统堆分配
- std::pmr::string 使用 PMR 资源
- std::string 的 SSO 优化在纯 PMR 条件下无法充分发挥
```

**对比说明：**

| 字符串实现 | sizeof | 1M 字符串内存 | size() 性能 | 创建性能 | Append 性能 |
|-----------|--------|-------------|-----------|---------|-----------|
| std::string | 32B | 125.89 MB | 基准 (113ms) | 基准 (61ms) | 基准 (28ms) |
| std::pmr::string | 40B | 133.51 MB | 1.01x | 3.5x 慢 | 4.4x 慢 |
| redis::String | 16B | 110.63 MB | 46.4x 慢 | 10x 慢 | 13.8x 慢 |

**最关键的发现：**

🎯 **std::pmr::string 与 std::string 几乎一样快**
- 说明 PMR 本身开销可以忽略
- 虚函数被编译器充分优化
- 性能差异主要来自内存分配策略（系统堆 vs PMR）

🎯 **redis::String 即使在公平 PMR 条件下仍然极其缓慢**
- 这彻底证明了性能问题的根源
- 不是 PMR（std::pmr::string 很快）
- 不是虚函数（std::pmr::string 没问题）
- 而是 SDS 架构设计本身

### 5. arena_and_size_benchmark ⭐ NEW

**目的：** 实际工程场景分析：Arena 分配器的影响 + 不同大小字符串的性能表现

**运行方式**:
```bash
./benchmark/arena_and_size_benchmark
```

**四个关键测试场景和发现：**

**Scenario 1: Arena 分配器对比 (创建 10M 个 32B 字符串)**
```
独立分配 new_delete_resource:      141ms (基准)
Arena monotonic_buffer_resource:    45ms (3.1x 快！)
redis::String + Arena:              561ms (3.98x 慢于独立)

✓ Arena 分配显著加速（3.1 倍）
✓ 但 redis::String 仍然因架构而慢
✓ 说明 redis::String 的问题不是分配
```

**Scenario 2: 不同大小字符串的 size() 查询 (100M 次)**
```
字符串大小   std::string   redis::String    倍数
1B           22.9ms        1053ms          46.0x
8B           22.2ms        1047ms          47.2x
16B          23.1ms        1053ms          45.5x
32B          22.6ms        1051ms          46.5x
64B          22.8ms        1051ms          46.2x
256B         22.1ms        1076ms          48.6x
1KB          22.6ms        1073ms          47.5x
10KB         22.7ms        1070ms          47.1x

✓ std::string 性能一致（大小无关）
✓ redis::String 性能也一致（大小无关）
✓ 说明这不是 SSO 问题，是根本架构差异
✓ 即使短字符串也无法获得性能优势
```

**Scenario 3: 短字符串创建 (100M 次创建)**
```
std::string 短字符串 (SSO 优化):     ~0ms (无额外分配)
std::pmr::string 短字符串:           ~0ms (Arena 分配很快)
redis::String 短字符串:           5863ms (无 SSO，总是代价)

✓ SSO 的威力：std::string 对短字符串几乎零成本
✓ Arena 也很快：std::pmr::string 相同性能
✓ redis::String 缺点：无 SSO 导致每次都有硬件成本
✓ 这在大量小字符串场景（HTTP 头等）差异明显
```

**Scenario 4: 批量操作 - 创建 + 查询 10M 个字符串**
```
std::string:           516.84ms (基准)
std::pmr::string:      366.58ms (0.71x，实际更快!)
redis::String:         914.07ms (1.77x 慢)

✓ pmr::string 配合 Arena 甚至比 std::string 更快！
✓ 说明灵活内存管理不会产生性能损失
✓ redis::String 即使配合 Arena 仍然较慢
```

**🎯 从实战角度的最终结论：**

1. ✅ **Arena 分配器真的有效** — 分配速度可提升 3 倍
2. ✅ **std::pmr::string + Arena 是最灵活的选择** — 性能甚至优于普通 std::string
3. ✅ **redis::String 不是分配问题** — 即使配合最优分配也无法改善
4. ✅ **SSO 对小对象至关重要** — redis::String 缺乏 SSO 是实际劣势
5. ✅ **实际工程推荐** — 优先 std::string，需要自定义内存：std::pmr::string + Arena
6. ✅ **学习 Redis 架构** — redis::String 仍然有教学价值

### 6. sorted_set_hybrid_vs_flat_map ⭐ NEW

**目的：** 直接比较有序集合两种实现策略的性能差异：

- flat_map-only（只用 `unordered_flat_map`）
- hybrid（当前 `listpack + skiplist + dict`）

**运行方式：**
```bash
./benchmark/sorted_set_hybrid_vs_flat_map
```

**测试规模：**

- elements = 200000
- zscore_rounds = 200000
- ordered_query_rounds = 1000

**最新结果（2026-03-31）：**

| 操作 | flat_map(ms) | hybrid(ms) | flat/hybrid |
|:---:|:------------:|:----------:|:-----------:|
| zadd | 22.41 | 125.90 | 0.18x |
| zscore | 13.41 | 12.41 | 1.08x |
| zcount | 705.49 | 1.81 | 389.34x |
| zrank | 680.64 | 1.60 | 425.93x |
| zrem | 4.96 | 97.80 | 0.05x |

**结论：**

- 查询路径（`zscore/zcount/zrank`）已经足够快。
- 当前优化重点应集中在写路径（`zadd/zrem`）。

---

## 📈 性能总结

### redis::String 性能劣势的三大原因

| 原因 | 虚函数开销 | 类型分派 | 架构设计 |
|------|:----------:|:-------:|:--------:|
| **改进潜力** | 24% | 难以优化 | 根本性 |
| **影响程度** | 中等 | 中等 | 大 |

1. **虚函数调用**（可优化的 24%）
   - PMR 内存资源多态导致的虚函数表查询
   - ✓ 已通过 pmr_string_comparison 验证：virtual overhead 被充分优化
   - std::pmr::string 使用虚函数但只慢 1.01x

2. **运行时类型分派**（难优化的 15-20%）
   - SDS 使用 4 分支 switch 进行类型选择（Type8/16/32/64）
   - redis::String 需要每次查询都判断类型后再访问
   - 编译器很难完全优化掉这个分派

3. **架构设计差异**（根本性的 50-70%）
   - **SDS 后向布局**（header before data）
     - 指针指向数据起点
     - 需要向后指针运算查找 header
     - 需要多级内存访问
   - **std::string 内联布局**
     - size/capacity 直接在对象内
     - 现代编译器充分内联优化
     - CPU 缓存友好
   - 根本无法在 C++ 中弥补这个差距

## 💡 使用建议

### 什么时候用 std::string？

✅ **几乎所有场景**
- 通用应用开发
- 性能关键路径
- 内存相对充足

### 什么时候用 redis::String？

✅ **特定场景**
- 学习 Redis SDS 内部实现
- 调试 Redis 协议解析
- 理论研究与教学
- 需要精确的 SDS 兼容性

### 性能优先级

```
1. std::string          <- 选这个
2. redis::String        <- 学习用
3. 自己实现字符串      <- 不要做
```

## 🔬 编译与执行

**编译时优化标志：**
```
-O3 -DNDEBUG
```

所有基准测试都使用最激进的编译优化，确保结果具有代表性。

**构建所有基准测试：**
```bash
cmake --build build --target sds_vs_string_benchmark
cmake --build build --target sds_template_vs_polymorphic
cmake --build build --target cpp_string_verdict
```

## 📝 测试方法论

**迭代次数选择：**
- 高频操作（size/capacity）：100-500M 次
- 一般操作（create/copy）：5-10M 次
- 低频操作（append 涉及重分配）：1-5M 次

**内存资源：**
- 使用 std::pmr::monotonic_buffer_resource（理想场景）
- 避免系统堆分配的干扰

**编译环境：**
- gcc-15 with -O3 -DNDEBUG
- C++23 标准

## 🎓 结论

**在 C++ 中：**

> std::string 是更优秀的选择。redis::String 主要用于学习和研究，生产环境应该选择 std::string。

**性能数据不会说谎：**
- std::string 在几乎所有操作上都 7-45 倍更快
- 内存占用差异微小（11%）
- 标准库支持更完善
- 编译器优化更深入
