# Sponge

Sponge 当前主要聚焦 Redis 模块。这是一个轻量的 C++23 Redis 风格服务实现，当前最值得关注的是简单命令路径的性能表现，以及围绕分片存储、TTL 和 AOF 的实现。

## Redis

- Redis 协议服务
- 分片存储与 TTL 管理
- String / SortedSet / ListPack 等基础结构
- AOF 与后台重写
- 基于 redis-benchmark 的吞吐与延迟对比

详细说明见 [docs/redis/README.md](docs/redis/README.md)，原始 benchmark 记录见 [docs/redis/BENCHMARK.md](docs/redis/BENCHMARK.md)。

## 性能对比

以下数据来自 2026-03-30 的同口径实测：原生 Redis 运行在 `6379`，Sponge RelWithDebInfo 运行在 `26379`。

基线命令：`redis-benchmark -c 50 -n 2000000 -t set,get -P 256 -r 100000`

### 吞吐量

| 场景 | 操作 | 原生 Redis | Sponge RelWithDebInfo | Sponge / Redis |
|:---:|:---:|:----------:|:---------------------:|:--------------:|
| 基线 3B | SET | 1.01M ops/s | 3.86M ops/s | **3.82x** |
| 基线 3B | GET | 2.13M ops/s | 5.73M ops/s | **2.69x** |
| 1KB value | SET | 299K ops/s | 1.42M ops/s | **4.75x** |
| 1KB value | GET | 809K ops/s | 2.98M ops/s | **3.68x** |
| 16KB value | SET | 35.8K ops/s | 183K ops/s | **5.11x** |
| 16KB value | GET | 177K ops/s | 1.02M ops/s | **5.76x** |

### 延迟

| 场景 | 操作 | 指标 | 原生 Redis | Sponge RelWithDebInfo |
|:---:|:---:|:---:|:----------:|:---------------------:|
| 基线 3B | SET | p50 | 11.431ms | 1.831ms |
| 基线 3B | SET | p99 | 24.799ms | 5.183ms |
| 基线 3B | GET | p50 | 5.527ms | 1.135ms |
| 基线 3B | GET | p99 | 9.303ms | 1.527ms |
| 1KB value | SET | p99 | 10.039ms | 1.903ms |
| 1KB value | GET | p99 | 5.743ms | 0.687ms |
| 16KB value | SET | p99 | 21.615ms | 3.719ms |
| 16KB value | GET | p99 | 3.959ms | 0.607ms |

这次 live 测试里，Sponge RelWithDebInfo 在基线、1KB 和 16KB 三组 `SET/GET` 上都显著快于原生 Redis，优势在 `SET` 路径和大 value 场景下尤其明显。

### 命令级结果

| 命令 | 原生 Redis throughput | Sponge throughput | Sponge / Redis | 原生 Redis p99 | Sponge p99 |
|:---:|:---------------------:|:----------------:|:--------------:|:--------------:|:----------:|
| EXISTS | 1.69M ops/s | 3.21M ops/s | **1.90x** | 2.935ms | 0.727ms |
| TYPE | 1.15M ops/s | 1.99M ops/s | **1.73x** | 2.239ms | 0.599ms |
| STRLEN | 1.17M ops/s | 2.11M ops/s | **1.80x** | 2.303ms | 0.567ms |
| DEL | 730K ops/s | 1.08M ops/s | **1.48x** | 1.927ms | 0.543ms |
| EXPIRE | 594K ops/s | 1.88M ops/s | **3.16x** | 4.695ms | 0.615ms |
| TTL | 1.15M ops/s | 1.99M ops/s | **1.73x** | 2.191ms | 0.559ms |
| PERSIST | 965K ops/s | 1.78M ops/s | **1.84x** | 3.279ms | 0.615ms |
| ZADD | 306K ops/s | 1.07M ops/s | **3.50x** | 1.703ms | 0.223ms |
| ZSCORE | 499K ops/s | 1.03M ops/s | **2.06x** | 1.023ms | 0.239ms |
| DBSIZE | 17.5K ops/s | 7.58K ops/s | **0.43x** | 0.079ms | 0.183ms |

除了 `DBSIZE` 这类控制面命令外，这轮 live 数据里，Sponge 在大多数已实现命令上也快于原生 Redis。

## 已实现命令

| 命令 | 类型 | 说明 |
|:----:|:----:|:----:|
| `SET key value` | Write | 设置字符串值 |
| `GET key` | Read | 获取字符串值 |
| `STRLEN key` | Read | 获取字符串长度 |
| `DEL key [key ...]` | Write | 删除键 |
| `EXISTS key` | Read | 检查键是否存在 |
| `EXPIRE key seconds` | Write | 设置过期时间 |
| `TTL key` | Read | 获取剩余 TTL |
| `PERSIST key` | Write | 移除过期时间 |
| `TYPE key` | Read | 获取值类型 |
| `RENAME key newkey` | Write | 重命名键 |
| `ZADD key score member` | Write | 向有序集合添加成员 |
| `ZSCORE key member` | Read | 获取成员分数 |
| `FLUSHALL` | Write | 清空所有数据 |
| `DBSIZE` | Read | 获取键数量 |
| `BGREWRITEAOF` | Write | 异步 AOF 重写 |

## 当前实现

- 存储层以分片 `DashTable` 为核心，结合 TTL 语义做并发访问控制。
- 值类型当前集中在字符串、整数和有序集合三条主路径。
- AOF 采用异步写入模型，并支持后台重写。
- 这轮 live benchmark 已覆盖 `SET/GET`、TTL 相关命令和部分有序集合路径，当前结论以实测结果为准。

如果只看 Redis，这个仓库建议从 [docs/redis/README.md](docs/redis/README.md)、[docs/redis/BENCHMARK.md](docs/redis/BENCHMARK.md) 和 [src/redis](src/redis) 开始读。