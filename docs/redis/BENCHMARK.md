# 复现实验

可以直接使用脚本批量跑完当前已实现命令的 benchmark，并把每一项输出保存到独立文件：

```bash
bash scripts/redis_benchmark_matrix.sh
```

默认目标是 `127.0.0.1:26379`。如果要对比原生 Redis：

```bash
PORT=6379 bash scripts/redis_benchmark_matrix.sh
```

如果要指定输出目录：

```bash
OUTPUT_DIR=benchmark-results/redis-debug bash scripts/redis_benchmark_matrix.sh
```

脚本覆盖的测试包括：

- SET / GET 基线
- 不同 value 大小
- 不同并发和 pipeline 深度
- EXISTS / TYPE / STRLEN / DEL
- EXPIRE / TTL / PERSIST
- RENAME
- ZADD / ZSCORE
- DBSIZE / FLUSHALL / BGREWRITEAOF

# 2026-03-30 Live 对比

以下结果来自同一台机器上的 live 实测：

- 原生 Redis：`127.0.0.1:6379`
- Sponge RelWithDebInfo：`127.0.0.1:26379`
- 压测工具：Docker 中的 `redis:7 redis-benchmark`

## 基线 SET / GET

命令：`redis-benchmark -c 50 -n 2000000 -P 256 -r 100000 -t set,get`

| 操作 | 原生 Redis | Sponge RelWithDebInfo | Sponge / Redis |
|:---:|:----------:|:---------------------:|:--------------:|
| SET throughput | 1.00M ops/s | 3.91M ops/s | **3.90x** |
| GET throughput | 2.09M ops/s | 5.78M ops/s | **2.77x** |
| SET p50 | 11.575ms | 1.647ms | **0.14x** |
| SET p95 | 17.519ms | 4.047ms | **0.23x** |
| SET p99 | 19.455ms | 5.271ms | **0.27x** |
| GET p50 | 5.639ms | 1.127ms | **0.20x** |
| GET p95 | 8.103ms | 1.431ms | **0.18x** |
| GET p99 | 9.255ms | 1.567ms | **0.17x** |

原始输出：

- [benchmark-results/redis-native-live-20260330/baseline_set_get.txt](benchmark-results/redis-native-live-20260330/baseline_set_get.txt)
- [benchmark-results/relwithdebinfo-live-20260330/baseline_set_get.txt](benchmark-results/relwithdebinfo-live-20260330/baseline_set_get.txt)

## 大小负载对比

### 1KB value

| 操作 | 原生 Redis | Sponge RelWithDebInfo | Sponge / Redis |
|:---:|:----------:|:---------------------:|:--------------:|
| SET throughput | 309K ops/s | 1.33M ops/s | **4.29x** |
| GET throughput | 822K ops/s | 3.03M ops/s | **3.68x** |
| SET p99 | 10.287ms | 1.839ms | **0.18x** |
| GET p99 | 5.583ms | 0.727ms | **0.13x** |

原始输出：

- [benchmark-results/redis-native-live-20260330/payload_1k_set_get.txt](benchmark-results/redis-native-live-20260330/payload_1k_set_get.txt)
- [benchmark-results/relwithdebinfo-live-20260330/payload_1k_set_get.txt](benchmark-results/relwithdebinfo-live-20260330/payload_1k_set_get.txt)

### 16KB value

| 操作 | 原生 Redis | Sponge RelWithDebInfo | Sponge / Redis |
|:---:|:----------:|:---------------------:|:--------------:|
| SET throughput | 36.1K ops/s | 182.8K ops/s | **5.07x** |
| GET throughput | 181.5K ops/s | 980.4K ops/s | **5.40x** |
| SET p99 | 23.407ms | 3.391ms | **0.14x** |
| GET p99 | 3.895ms | 0.615ms | **0.16x** |

原始输出：

- [benchmark-results/redis-native-live-20260330/payload_16k_set_get.txt](benchmark-results/redis-native-live-20260330/payload_16k_set_get.txt)
- [benchmark-results/relwithdebinfo-live-20260330/payload_16k_set_get.txt](benchmark-results/relwithdebinfo-live-20260330/payload_16k_set_get.txt)

## 低并发单请求

命令：`redis-benchmark -c 1 -n 200000 -P 1 -r 100000 -t set,get`

| 操作 | 原生 Redis | Sponge RelWithDebInfo | Sponge / Redis |
|:---:|:----------:|:---------------------:|:--------------:|
| SET throughput | 15.7K ops/s | 33.5K ops/s | **2.14x** |
| GET throughput | 16.5K ops/s | 35.3K ops/s | **2.13x** |
| SET p99 | 0.095ms | 0.047ms | **0.49x** |
| GET p99 | 0.095ms | 0.047ms | **0.49x** |

原始输出：

- [benchmark-results/redis-native-live-20260330/concurrency_c1_p1_set_get.txt](benchmark-results/redis-native-live-20260330/concurrency_c1_p1_set_get.txt)
- [benchmark-results/relwithdebinfo-live-20260330/concurrency_c1_p1_set_get.txt](benchmark-results/relwithdebinfo-live-20260330/concurrency_c1_p1_set_get.txt)

## 命令级结果

这一轮已经补齐了大部分原生 Redis 命令级结果，可以和 Sponge RelWithDebInfo 做直接对比：

| 命令 | 原生 Redis throughput | Sponge throughput | Sponge / Redis | 原生 Redis p99 | Sponge p99 |
|:---:|:---------------------:|:----------------:|:--------------:|:--------------:|:----------:|
| EXISTS | 1.77M ops/s | 3.52M ops/s | **2.00x** | 2.703ms | 0.703ms |
| TYPE | 1.23M ops/s | 1.92M ops/s | **1.56x** | 2.447ms | 0.591ms |
| STRLEN | 1.22M ops/s | 1.96M ops/s | **1.61x** | 2.087ms | 0.559ms |
| DEL | 754.7K ops/s | 1.03M ops/s | **1.36x** | 1.935ms | 0.535ms |
| EXPIRE | 592.9K ops/s | 1.80M ops/s | **3.03x** | 约 5.1ms | 0.623ms |
| TTL | 1.13M ops/s | 2.05M ops/s | **1.81x** | 2.215ms | 0.591ms |
| PERSIST | 928.8K ops/s | 1.86M ops/s | **2.01x** | 3.039ms | 0.599ms |
| ZADD | 384.6K ops/s | 1.03M ops/s | **2.68x** | 1.343ms | 0.231ms |
| ZSCORE | 647.2K ops/s | 1.08M ops/s | **1.67x** | 0.815ms | 0.239ms |
| DBSIZE | 17.2K ops/s | 7.94K ops/s | **0.46x** | 0.079ms | 0.175ms |

可以看到，除了 `DBSIZE` 这种明显偏控制面、且实现策略差异很大的命令外，这轮实测里 Sponge 在大多数已实现命令上都快于原生 Redis。

## 特殊命令说明

- `RENAME`：原生 Redis 这轮 benchmark 使用 `key:__rand_int__ -> key2:__rand_int__` 时直接返回 `ERR no such key`，因此这组结果无效；Sponge 侧得到的是 561.8K ops/s，p99 0.215ms，但当前不能做公平对照。
- `FLUSHALL`：原生 Redis 跑通了，结果是 256.41 ops/s，p99 4.159ms；Sponge 这轮在尾部因为 AOF 进入 `MISCONF` 状态被阻断，无法直接比较。
- `BGREWRITEAOF`：原生 Redis 返回 `ERR Background append only file rewriting already in progress`；Sponge 这轮则在尾部 AOF 异常后退出，因此这项目前不适合写成性能表。

原始输出：

- [benchmark-results/redis-native-live-20260330/exists.txt](benchmark-results/redis-native-live-20260330/exists.txt)
- [benchmark-results/redis-native-live-20260330/type.txt](benchmark-results/redis-native-live-20260330/type.txt)
- [benchmark-results/redis-native-live-20260330/strlen.txt](benchmark-results/redis-native-live-20260330/strlen.txt)
- [benchmark-results/redis-native-live-20260330/del.txt](benchmark-results/redis-native-live-20260330/del.txt)
- [benchmark-results/redis-native-live-20260330/expire.txt](benchmark-results/redis-native-live-20260330/expire.txt)
- [benchmark-results/redis-native-live-20260330/ttl.txt](benchmark-results/redis-native-live-20260330/ttl.txt)
- [benchmark-results/redis-native-live-20260330/persist.txt](benchmark-results/redis-native-live-20260330/persist.txt)
- [benchmark-results/redis-native-live-20260330/zadd.txt](benchmark-results/redis-native-live-20260330/zadd.txt)
- [benchmark-results/redis-native-live-20260330/zscore.txt](benchmark-results/redis-native-live-20260330/zscore.txt)
- [benchmark-results/redis-native-live-20260330/dbsize.txt](benchmark-results/redis-native-live-20260330/dbsize.txt)
- [benchmark-results/redis-native-live-20260330/rename.txt](benchmark-results/redis-native-live-20260330/rename.txt)
- [benchmark-results/redis-native-live-20260330/flushall.txt](benchmark-results/redis-native-live-20260330/flushall.txt)
- [benchmark-results/redis-native-live-20260330/bgrewriteaof.txt](benchmark-results/redis-native-live-20260330/bgrewriteaof.txt)

原始输出目录：

- [benchmark-results/redis-native-live-20260330](benchmark-results/redis-native-live-20260330)
- [benchmark-results/relwithdebinfo-live-20260330](benchmark-results/relwithdebinfo-live-20260330)

## 备注

这次 Sponge live 压测在尾部阶段再次触发了 AOF 健康问题，`FLUSHALL` 返回了 `MISCONF AOF is configured to save data, but is currently not able to persist on disk`，随后服务退出。因此 `BGREWRITEAOF` 没有得到完整结果，但前面的性能数据已经落盘且可用。

---

# 2026-03-30 Live 对比（Run 2）

本轮为 AOF rewrite 逻辑改动（CAS guard / rewrite buffer / buffer replay）后重新编译的 RelWithDebInfo 版本，与上一轮使用完全相同的测试参数，以便直接对照。

- 原生 Redis：`127.0.0.1:6379`
- Sponge RelWithDebInfo (post-AOF-fix)：`127.0.0.1:26379`
- 压测工具：Docker `redis:7 redis-benchmark`

## 基线 SET / GET

命令：`redis-benchmark -c 50 -n 2000000 -P 256 -r 100000 -t set,get`

| 操作 | 原生 Redis | Sponge RelWithDebInfo | Sponge / Redis |
|:---:|:----------:|:---------------------:|:--------------:|
| SET throughput | 1.01M ops/s | 3.86M ops/s | **3.82x** |
| GET throughput | 2.13M ops/s | 5.73M ops/s | **2.69x** |
| SET p50 | 11.431ms | 1.831ms | **0.16x** |
| SET p99 | 24.799ms | 5.183ms | **0.21x** |
| GET p50 | 5.527ms | 1.135ms | **0.21x** |
| GET p99 | 9.303ms | 1.527ms | **0.16x** |

## 大小负载对比

### 1KB value

| 操作 | 原生 Redis | Sponge RelWithDebInfo | Sponge / Redis |
|:---:|:----------:|:---------------------:|:--------------:|
| SET throughput | 299K ops/s | 1.42M ops/s | **4.75x** |
| GET throughput | 809K ops/s | 2.98M ops/s | **3.68x** |
| SET p99 | 10.039ms | 1.903ms | **0.19x** |
| GET p99 | 5.743ms | 0.687ms | **0.12x** |

### 16KB value

| 操作 | 原生 Redis | Sponge RelWithDebInfo | Sponge / Redis |
|:---:|:----------:|:---------------------:|:--------------:|
| SET throughput | 35.8K ops/s | 183K ops/s | **5.11x** |
| GET throughput | 177K ops/s | 1.02M ops/s | **5.76x** |
| SET p99 | 21.615ms | 3.719ms | **0.17x** |
| GET p99 | 3.959ms | 0.607ms | **0.15x** |

## 命令级全对比

所有命令均在 200k–300k 请求、`-c 50 -P 32` 条件下测试；ZADD/ZSCORE 使用 `-c 20 -P 16`；DBSIZE 使用 `-c 1 -n 1000`。

| 命令 | 原生 Redis | Sponge | Sponge / Redis | 原生 Redis p99 | Sponge p99 |
|:---:|:----------:|:------:|:--------------:|:--------------:|:----------:|
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

> **DBSIZE 说明**：原生 Redis 为 O(1) 实现（直接读计数器）；Sponge 当前实现需要遍历各分片累加，属于 O(shards) 路径，因此在 DBSIZE 上低于原生 Redis。其他所有数据面命令 Sponge 均具备显著优势。

原始输出目录：

- [benchmark-results/sponge-20260330-2](../../benchmark-results/sponge-20260330-2)
- [benchmark-results/redis-native-20260330-2](../../benchmark-results/redis-native-20260330-2)
## FLUSHALL / BGREWRITEAOF 说明

### FLUSHALL

| | 原生 Redis | Sponge |
|:---:|:---:|:---:|
| throughput | **258.93 ops/s** | 无法测量（见下） |
| p50 | 3.839ms | — |
| p99 | 4.239ms | — |

测试条件：`-c 1 -n 1000 FLUSHALL`

**Sponge FLUSHALL 无法用 redis-benchmark 测量的原因：** `command::flushall` 执行时调用 `AOF::reset()`，`reset()` 在重置开头无条件执行 `healthy_.store(false)` 再在完成后恢复 true。当 redis-benchmark 收到上一条 FLUSHALL 的 OK 后立即发下一条时，io_context 线程仍在执行 reset 任务（`healthy_` 仍为 false），下一条 FLUSHALL 的 `is_healthy()` 检查读到 false → 返回 MISCONF，benchmark 中止。这不是真实 I/O 错误，而是 `reset()` 把"正在重置"与"写入出错"共用同一个 flag 导致的设计问题。

### BGREWRITEAOF

两侧均无法用 redis-benchmark 正常测量：

- **原生 Redis**：直接返回 `ERR Background append only file rewriting already in progress`；AOF 重写是串行后台操作，并发请求几乎全部失败。
- **Sponge**：同 FLUSHALL，触发相同的 MISCONF 路径（BGREWRITEAOF 是 Write 命令，同样经过 `is_healthy()` 检查）。

BGREWRITEAOF 本质上是一次性后台操作，不适合用吞吐量指标衡量；正确的对比方式是对比重写完成时间（rewrite latency），需要单独脚本实现。
