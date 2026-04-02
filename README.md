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

以下数据来自 2026-03-31 的同口径实测：

- Sponge RelWithDebInfo（AOF everysec）：`127.0.0.1:26379`
- 原生 Redis 7：`127.0.0.1:6379`
- 压测工具：Docker `redis:7 redis-benchmark`

基线命令：`redis-benchmark -c 50 -n 2000000 -t set,get -P 256 -r 100000`

### 基线吞吐

| 操作 | 原生 Redis | Sponge | Sponge / Redis |
|:---:|:----------:|:------:|:--------------:|
| SET | 1.23M ops/s | 5.87M ops/s | **4.77x** |
| GET | 2.42M ops/s | 7.84M ops/s | **3.25x** |

### 负载分层（SET/GET）

| payload | 操作 | 原生 Redis | Sponge | Sponge / Redis |
|:-------:|:---:|:----------:|:------:|:--------------:|
| 16B | SET | 1.14M | 3.68M | **3.24x** |
| 16B | GET | 2.09M | 5.53M | **2.65x** |
| 256B | SET | 726K | 1.37M | **1.89x** |
| 256B | GET | 1.68M | 4.85M | **2.90x** |
| 1KB | SET | 385K | 434K | **1.13x** |
| 1KB | GET | 988K | 1.89M | **1.92x** |
| 4KB | SET | 119K | 74.9K | **0.63x** |
| 4KB | GET | 446K | 656K | **1.47x** |
| 16KB | SET | 40.5K | 34.9K | **0.86x** |
| 16KB | GET | 189K | 233K | **1.23x** |

### 命令级摘要

- 优势明显：`MSET` 3.26x、`HSET` 3.36x、`LPUSH` 3.12x、`INCR` 2.77x
- 持平或略优：`HGET` 1.56x、`LRANGE` 1.80x、`ZRANGE` 1.52x
- 当前主要劣势：`ZADD`、`ZREM` 这类写路径，以及 `DBSIZE`（控制面统计）

完整命令级对比和原始输出见 [docs/redis/BENCHMARK.md](docs/redis/BENCHMARK.md)。

### Docker 端口 16379 实测（2026-03-31）

补充一组 Docker 场景结果（目标 `127.0.0.1:16379`，压测工具仍为 Docker `redis:7 redis-benchmark`）。

结果目录：

- [benchmark-results/port-16379-20260331](benchmark-results/port-16379-20260331)

#### 基线 SET/GET

| 操作 | throughput | p50 | p95 | p99 |
|:---:|:----------:|:---:|:---:|:---:|
| SET | 5.83M ops/s | 1.183ms | 1.655ms | 2.231ms |
| GET | 6.49M ops/s | 1.055ms | 1.199ms | 1.303ms |

#### payload 分层（SET/GET）

| payload | 操作 | throughput | p99 |
|:-------:|:---:|:----------:|:---:|
| 1KB | SET | 360K ops/s | 1.527ms |
| 1KB | GET | 1.66M ops/s | 1.335ms |
| 16KB | SET | 30.8K ops/s | 2.815ms |
| 16KB | GET | 214K ops/s | 1.247ms |

#### 命令级摘录

| 命令 | throughput | p99 |
|:---:|:----------:|:---:|
| EXISTS | 3.57M ops/s | 0.663ms |
| STRLEN | 2.21M ops/s | 0.511ms |
| EXPIRE | 2.11M ops/s | 0.511ms |
| TTL | 2.13M ops/s | 0.575ms |
| PERSIST | 2.08M ops/s | 0.607ms |
| ZADD | 1.12M ops/s | 0.239ms |
| ZSCORE | 1.14M ops/s | 0.239ms |
| DBSIZE | 7.09K ops/s | 0.215ms |

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
| `ZCOUNT key min max` | Read | 统计分数区间内成员数量 |
| `ZRANK key member` | Read | 获取成员排名（从 0 开始） |
| `ZREM key member` | Write | 删除有序集合成员 |
| `FLUSHALL` | Write | 清空所有数据 |
| `DBSIZE` | Read | 获取键数量 |
| `BGREWRITEAOF` | Write | 异步 AOF 重写 |

## 当前实现

- 存储层以分片 `DashTable` 为核心，结合 TTL 语义做并发访问控制。
- 值类型当前集中在字符串、整数和有序集合三条主路径。
- AOF 采用异步写入模型，并支持后台重写。
- 这轮 live benchmark 已覆盖 `SET/GET`、TTL 相关命令和部分有序集合路径，当前结论以实测结果为准。

如果只看 Redis，这个仓库建议从 [docs/redis/README.md](docs/redis/README.md)、[docs/redis/BENCHMARK.md](docs/redis/BENCHMARK.md) 和 [app/redis](app/redis) 开始读。

## Docker
docker build -t sponge-redis:v1.0-jemalloc .