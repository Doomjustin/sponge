# Redis 命令整理（按官方语义）

本文档按 Redis 官方常见用法整理命令，重点说明三件事：

- 命令做了什么
- 常用选项含义
- 返回值语义（尤其是容易混淆的部分）

说明：

- 以 Redis 7.x 常见行为为准。
- 不是完整手册，优先覆盖高频和易错命令。

## 1. 键空间与基础

### EXISTS key [key ...]

作用：检查一个或多个 key 是否存在。

返回：存在的 key 个数（整数）。

要点：多个 key 时按“存在个数”计数，不是布尔值。

### DEL key [key ...]

作用：删除一个或多个 key。

返回：实际删除的 key 个数。

要点：不存在的 key 不计数。

### UNLINK key [key ...]

作用：异步回收内存的删除（逻辑删除立即生效，内存释放后台做）。

返回：被 unlink 的 key 个数。

适用：大 key 删除，减少主线程阻塞。

### TYPE key

作用：返回 key 的数据类型。

返回：`string` / `list` / `set` / `zset` / `hash` / `stream`，不存在时 `none`。

### RENAME key newkey

作用：重命名 key。

行为：如果 `newkey` 已存在，会被覆盖。

### RENAMENX key newkey

作用：仅当 `newkey` 不存在时重命名。

返回：1 成功，0 失败。

## 2. 过期与生存时间

### EXPIRE key seconds
### PEXPIRE key milliseconds

作用：设置过期时间。

返回：1 设置成功，0（key 不存在或条件不满足）。

常用选项（Redis 7 常见）：

- `NX`：仅当 key 当前没有过期时间时设置
- `XX`：仅当 key 当前已有过期时间时设置
- `GT`：仅当新过期时间比当前更晚时设置
- `LT`：仅当新过期时间比当前更早时设置

### EXPIREAT key unix_time_seconds
### PEXPIREAT key unix_time_milliseconds

作用：设置绝对过期时间戳。

### TTL key
### PTTL key

作用：查看剩余生存时间。

返回语义：

- `> 0`：剩余时间（TTL 单位秒，PTTL 单位毫秒）
- `-1`：key 存在但无过期时间（persistent）
- `-2`：key 不存在

### PERSIST key

作用：移除过期时间，使 key 永久存在。

返回：1 成功，0 失败（key 不存在或本来就没有过期）。

## 3. 字符串 String

### SET key value [选项]

作用：写入字符串值。

常用选项：

- `EX seconds`：秒级过期
- `PX milliseconds`：毫秒级过期
- `EXAT unix_time_seconds`：秒级绝对时间
- `PXAT unix_time_milliseconds`：毫秒级绝对时间
- `NX`：仅当 key 不存在时写入
- `XX`：仅当 key 已存在时写入
- `KEEPTTL`：保留原有过期时间
- `GET`：写入前先返回旧值

返回：通常 `OK`；若配合 `NX`/`XX` 不满足条件则返回空。

### GET key

作用：读取字符串值。

返回：值或空（nil）。

### MSET key value [key value ...]

作用：批量设置多个键值。

返回：`OK`。

### MGET key [key ...]

作用：批量读取。

返回：数组；不存在项为 nil。

### INCR / DECR / INCRBY / DECRBY

作用：把字符串按整数做原子增减。

前提：值必须是可解析整数。

### APPEND key value

作用：在旧值末尾拼接。

返回：拼接后字符串长度。

## 4. 哈希 Hash

### HSET key field value [field value ...]

作用：设置一个或多个字段。

返回：新增字段个数（覆盖已有字段不计新增）。

### HGET key field

作用：读取字段值。

返回：值或 nil。

### HMSET / HMGET

现代用法：一般使用多字段 `HSET` 和 `HMGET`。

### HINCRBY key field increment

作用：对 hash 字段做整数原子增减。

### HDEL key field [field ...]

作用：删除字段。

返回：删除字段个数。

### HGETALL key

作用：获取全部字段和值。

返回：扁平数组（field1, value1, field2, value2 ...）。

## 5. 列表 List

### LPUSH / RPUSH key element [element ...]

作用：头/尾插入元素。

返回：插入后列表长度。

### LPOP / RPOP key [count]

作用：头/尾弹出。

返回：单值或数组（指定 count 时）。

### LRANGE key start stop

作用：按区间取元素（包含 stop）。

要点：支持负索引（-1 表示最后一个）。

### LLEN key

作用：列表长度。

## 6. 集合 Set

### SADD key member [member ...]

作用：添加成员。

返回：新增成员个数。

### SREM key member [member ...]

作用：删除成员。

返回：删除个数。

### SMEMBERS key

作用：返回全部成员。

### SISMEMBER key member

作用：检查成员是否存在。

返回：1/0。

### SCARD key

作用：集合基数（成员数）。

## 7. 有序集合 ZSet

### ZADD key [选项] score member [score member ...]

作用：按 score 写入成员。

常用选项：

- `NX`：仅新增，不更新已有成员
- `XX`：仅更新，不新增
- `CH`：返回值统计“变化数量”（新增 + 分数变化）
- `INCR`：把操作转成增量更新（一次只处理一个 member）

返回：默认返回新增成员个数（受选项影响）。

### ZRANGE key start stop [WITHSCORES]

作用：按排名范围取成员。

`WITHSCORES`：同时返回分数。

### ZRANGEBYSCORE key min max [WITHSCORES] [LIMIT offset count]

作用：按分数范围取成员。

### ZREM key member [member ...]

作用：删除成员。

### ZSCORE key member

作用：取成员分数。

### ZCARD key

作用：有序集合成员数量。

## 8. 事务与乐观锁

### MULTI

作用：开启事务队列。

### EXEC

作用：执行事务队列中的命令。

返回：每条命令的结果数组。

### DISCARD

作用：放弃事务队列。

### WATCH key [key ...]

作用：监视 key，配合 MULTI/EXEC 做乐观锁。

语义：被监视 key 在 EXEC 前改动则事务失败（返回空结果）。

## 9. 发布订阅 Pub/Sub

### PUBLISH channel message

作用：向频道发布消息。

返回：收到消息的订阅者数量。

### SUBSCRIBE channel [channel ...]
### PSUBSCRIBE pattern [pattern ...]

作用：订阅频道或模式。

注意：连接进入订阅模式后只接收订阅相关流。

## 10. 服务端与客户端常见命令

### PING [message]

作用：连通性检查。

返回：`PONG` 或自定义 message。

### AUTH [username] password

作用：认证。

### SELECT index

作用：切换逻辑库（默认 0）。

### INFO [section]

作用：查看服务器状态。

### CONFIG GET parameter
### CONFIG SET parameter value

作用：读取/设置配置（受权限和配置限制）。

## 11. 常见返回类型速查

- 简单字符串：如 `+OK`
- 错误：如 `-ERR ...`
- 整数：如 `:1`
- Bulk String：如 `$3\r\nfoo`
- Null（RESP2）：`$-1`
- 数组：`*N ...`

## 12. 实战选项解释（高频）

### SET 的 NX / XX

- `NX`：只创建，不覆盖
- `XX`：只覆盖，不创建

典型用途：

- `SET lock_id token NX PX 30000`：分布式锁（基础形态）

### EXPIRE 的 NX / XX / GT / LT

- `NX`：只给“无 TTL”键设置 TTL
- `XX`：只更新“已有 TTL”键
- `GT`：只允许延长 TTL
- `LT`：只允许缩短 TTL

### ZADD 的 NX / XX / CH / INCR

- `NX`/`XX`：控制新增或仅更新
- `CH`：统计“变化”而不只是“新增”
- `INCR`：将 score 当增量

## 13. 兼容性与实践建议

- TTL/PTTL 的 `-2/-1/>=0` 语义建议严格保持，客户端普遍依赖。
- 读写混合高并发下，优先保证“检查与修改在同一临界区”，避免竞态。
- 对大 key 删除优先考虑 `UNLINK`，避免阻塞。
- 业务文档里建议写清楚命令支持范围（实现子集）和未实现项。
