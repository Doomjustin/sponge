# Sponge Redis 模块

一个轻量 C++23 Redis 兼容服务实现，重点关注分片存储、TTL 管理和协议支持。

## 目录

- [架构](#架构)
- [核心组件](#核心组件)
- [性能基准](#性能基准)
- [支持的命令](#支持的命令)

## 架构

```
┌─────────────────────────────────────────┐
│           Server/Session                │  协议处理、连接管理
├─────────────────────────────────────────┤
│        RESP Protocol Parser             │  标准 Redis 协议
├─────────────────────────────────────────┤
│       ApplicationContext                │  全局资源管理
├─────────────────────────────────────────┤
│  DBShard × N (1024 segments/shard)     │  分片存储、并发控制
├─────────────────────────────────────────┤
│     DashTable + TTLManager              │  键值存储、过期处理
├─────────────────────────────────────────┤
│   String / SDS / SortedSet / ListPack  │  数据结构
├─────────────────────────────────────────┤
│     AOF + Boost.Asio I/O Thread        │  异步持久化
└─────────────────────────────────────────┘
```

## 核心组件

### 1. DashTable（分段哈希表）

- **分段数**：1024 个独立段
- **锁策略**：每段一把 `shared_mutex`（读写分离）
- **键类型**：PMR string
- **值类型**：泛型，支持 string / int / sorted_set
- **异构查找**：支持 `std::string_view` 查找

**关键接口**：
- `modify(key, func)`：独占访问，执行回调
- `get(key)`：读取
- `set(key, value)`：写入
- `range(read_only|read_write)`：范围迭代

### 2. DBShard（单分片）

- 封装 DashTable + TTLManager
- 提供 `modify(key, callback)` 接口，执行前自动过期清理
- 支持字符串、整数、有序集合三种值类型

### 3. TTLManager（过期管理）

- 原子时间戳存储
- 支持永久键（负数）和有期键
- 提供 `is_expired()`, `ttl()`, `expire_at()` 工具函数

### 4. AOF（追加写日志）

- **异步模型**：单后台 I/O 线程（Boost.Asio）
- **接口**：`append(command)` 只做一次 post，立即返回
- **健康检查**：`is_healthy()` 查询写入通道状态
- **后台重写**：支持 BGREWRITEAOF 命令

### 5. Server/Session

- **协议**：完整 RESP protocol 支持
- **线程模型**：Boost.Asio 协程 + 每线程一个 io_context
- **连接处理**：异步 socket accept + 协程处理会话

## 性能基准

**测试条件**：
- 原生 Redis：`127.0.0.1:6379`
- Sponge RelWithDebInfo：`127.0.0.1:26379`
- 命令：`redis-benchmark -c 50 -n 2000000 -t set,get -P 256 -r 100000`
- 客户端：50 并发
- 管道深度：256
- 数据来源：2026-03-30 live 实测，完整日志见 [docs/redis/BENCHMARK.md](docs/redis/BENCHMARK.md)

### 吞吐量对比

| 场景 | 操作 | 原生 Redis | Sponge RelWithDebInfo | Sponge / Redis |
|:---:|:---:|:----------:|:---------------------:|:--------------:|
| 基线 3B | SET | 1.01M ops/s | 3.86M ops/s | **3.82x** |
| 基线 3B | GET | 2.13M ops/s | 5.73M ops/s | **2.69x** |
| 1KB value | SET | 299K ops/s | 1.42M ops/s | **4.75x** |
| 1KB value | GET | 809K ops/s | 2.98M ops/s | **3.68x** |
| 16KB value | SET | 35.8K ops/s | 183K ops/s | **5.11x** |
| 16KB value | GET | 177K ops/s | 1.02M ops/s | **5.76x** |

### 延迟对比

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

这次 live 测试里，Sponge RelWithDebInfo 在基线、1KB 和 16KB 三组 `SET/GET` 上都显著快于原生 Redis；命令级结果里，除了 `DBSIZE` 这类控制面命令外，大多数已实现命令也都有优势。

## 支持的命令

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
