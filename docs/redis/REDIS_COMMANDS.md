# Redis 推荐命令清单（服务器端 RESP 返回语义）

本文档按 Redis 服务器端 RESP 返回语义描述，不使用客户端展示词（如 `nil`）。

RESP 记号约定：

- Simple String：`+...\r\n`
- Error：`-...\r\n`
- Integer：`:n\r\n`
- Bulk String：`$len\r\n...\r\n`
- Null Bulk String：`$-1\r\n`
- Array：`*N\r\n...`
- Null Array：`*-1\r\n`

## 1. Key 与过期

### EXISTS key [key ...]
作用：统计存在的 key 数量。

返回：Integer，`:count`。

### DEL key [key ...]
作用：删除 key。

返回：Integer，`:deleted_count`。

### UNLINK key [key ...]
作用：异步删除，适合大 key。

返回：Integer，`:scheduled_count`。

### TYPE key
作用：返回 key 类型（不存在时 `none`）。

返回：Bulk String，如 `$6\r\nstring\r\n`、`$4\r\nnone\r\n`。

### RENAME key newkey
作用：把 `key` 重命名为 `newkey`。

返回：

- 成功：`+OK\r\n`
- 失败（源 key 不存在）：`-ERR no such key\r\n`

### RENAMENX key newkey
作用：仅当 `newkey` 不存在时执行重命名。

返回：Integer。

- 成功：`:1`
- 未执行：`:0`

### EXPIRE key seconds [NX|XX|GT|LT]
### PEXPIRE key milliseconds [NX|XX|GT|LT]
作用：设置过期时间。

返回：Integer。

- 成功设置：`:1`
- 未设置（key 不存在或条件不满足）：`:0`

### EXPIRETIME key
### PEXPIRETIME key
作用：返回绝对过期时间戳（秒/毫秒）。

返回：Integer。

- 有过期时间：`:timestamp`
- 无过期时间：`:-1`
- key 不存在：`:-2`

### TTL key
### PTTL key
作用：返回剩余过期时间（秒/毫秒）。

返回：Integer。

- 有过期时间：`:remaining`
- 无过期时间：`:-1`
- key 不存在：`:-2`

### PERSIST key
作用：移除过期时间。

返回：Integer。

- 成功移除：`:1`
- 未移除（key 不存在或无过期）：`:0`

## 2. String

### SET key value [EX seconds|PX milliseconds|EXAT ts|PXAT tsms] [NX|XX] [KEEPTTL] [GET]
作用：设置字符串值。

返回：

- 默认成功：`+OK\r\n`
- 使用 `NX/XX` 且条件不满足：`$-1\r\n`（RESP2）
- 使用 `GET`：返回旧值（Bulk String 或 Null Bulk String）

### GET key
作用：读取字符串值。

返回：

- 命中：Bulk String
- key 不存在：`$-1\r\n`
- 类型不匹配：`-WRONGTYPE ...\r\n`

### MSET key value [key value ...]
作用：批量设置。

返回：`+OK\r\n`。

### MGET key [key ...]
作用：批量读取。

返回：Array，元素按请求顺序对应。

- 命中元素：Bulk String
- 未命中元素：Null Bulk String（`$-1\r\n`）

### INCR key / DECR key / INCRBY key n / DECRBY key n
作用：整数原子增减。

返回：Integer，增减后的新值。

错误：

- 值非整数：`-ERR value is not an integer or out of range\r\n`
- 溢出：同类 `-ERR ... out of range\r\n`

### APPEND key value
作用：字符串尾部拼接。

返回：Integer，拼接后字符串长度。

### STRLEN key
作用：返回字符串长度。

返回：Integer。

- key 不存在：`:0`

## 3. Hash

### HSET key field value [field value ...]
作用：设置一个或多个字段。

返回：Integer，本次新增 field 数量（覆盖不计）。

### HGET key field
作用：读取字段值。

返回：

- 命中：Bulk String
- 未命中：`$-1\r\n`
- 类型不匹配：`-WRONGTYPE ...\r\n`

### HMGET key field [field ...]
作用：批量读取字段。

返回：Array。

- 命中字段：Bulk String
- 未命中字段：Null Bulk String

### HINCRBY key field increment
作用：字段整数原子增减。

返回：Integer，增减后的新值。

错误：字段值非整数或溢出时返回 `-ERR ...\r\n`。

### HDEL key field [field ...]
作用：删除字段。

返回：Integer，实际删除字段数。

### HGETALL key
作用：读取全部字段和值。

返回：Array，形如 `*{2N}` 后续按 `field, value` 成对输出。

- key 不存在：`*0\r\n`

### HKEYS key
作用：返回所有 field 名称。

返回：Array（Bulk String 列表）。

- key 不存在：`*0\r\n`
- 类型不匹配：`-ERR ...\r\n`

### HVALS key
作用：返回所有 field 的值。

返回：Array（Bulk String 列表）。

- key 不存在：`*0\r\n`
- 类型不匹配：`-ERR ...\r\n`

### HEXISTS key field
作用：判断 field 是否存在。

返回：Integer。

- 存在：`:1`
- 不存在或 key 不存在：`:0`
- 类型不匹配：`-ERR ...\r\n`

## 4. List

### LPUSH key element [element ...]
### RPUSH key element [element ...]
作用：头/尾插入。

返回：Integer，插入后列表长度。

### LPOP key [count]
### RPOP key [count]
作用：头/尾弹出。

返回：

- 不带 `count`：Bulk String 或 Null Bulk String
- 带 `count`：Array（可空 `*0\r\n`）

### LRANGE key start stop
作用：按下标范围读取。

返回：Array（可空）。

### LLEN key
作用：长度。

返回：Integer（key 不存在时 `:0`）。

### LINDEX key index
作用：按下标读取单个元素，支持负数索引（`-1` 为尾部）。

返回：

- 命中：Bulk String
- 越界或 key 不存在：`$-1\r\n`
- 类型不匹配：`-ERR ...\r\n`

### LSET key index value
作用：按下标覆写元素。

返回：

- 成功：`+OK\r\n`
- 越界：`-ERR index out of range\r\n`
- key 不存在：`-ERR no such key\r\n`
- 类型不匹配：`-ERR ...\r\n`

### LTRIM key start stop
作用：裁剪列表只保留 `[start, stop]` 范围（支持负数索引）。收缩为空时自动删除 key。

返回：`+OK\r\n`。

- key 不存在：`+OK\r\n`（视为空操作）
- 类型不匹配：`-ERR ...\r\n`
### BLMPOP timeout numkeys key [key ...] LEFT|RIGHT [COUNT count]
作用：多 key 弹出（非阻塞/阻塞）。

返回：

- 命中：Array，结构为 `[key, [elem1, elem2, ...]]`
- 未命中：Null Array（`*-1\r\n`，阻塞命令超时同理）

## 5. Set

### SADD key member [member ...]
作用：添加成员。

返回：Integer，新增成员数量。

### SREM key member [member ...]
作用：删除成员。

返回：Integer，删除成员数量。

### SMEMBERS key
作用：获取所有成员。

返回：Array（可空）。

### SISMEMBER key member
作用：成员是否存在。

返回：Integer，存在 `:1`，不存在 `:0`。

### SCARD key
作用：成员数。

返回：Integer（key 不存在时 `:0`）。

### SINTERCARD numkeys key [key ...] [LIMIT limit]
作用：求交集基数（不返回具体成员）。

返回：Integer。

## 6. ZSet

### ZADD key [NX|XX] [CH] [INCR] score member [score member ...]
作用：写入有序集合。

返回：

- 普通模式：Integer（新增数量；带 `CH` 时为变更数量）
- `INCR` 模式：Bulk String（新分数）或 Null Bulk String（条件不满足）

### ZRANGE key start stop [REV] [BYSCORE|BYLEX] [LIMIT offset count] [WITHSCORES]
作用：统一范围查询入口。

返回：

- 默认：Array（成员列表）
- `WITHSCORES`：Array（`member, score, member, score, ...`）

### ZREM key member [member ...]
作用：删除成员。

返回：Integer，删除数量。

### ZSCORE key member
作用：读取成员分数。

返回：Bulk String（分数字符串）或 Null Bulk String。

### ZCARD key
作用：成员数量。

返回：Integer（key 不存在时 `:0`）。

### ZCOUNT key min max
作用：统计分数值在 `[min, max]` 区间内的成员数量。

返回：Integer（key 不存在时 `:0`）。

- 类型不匹配：`-ERR ...\r\n`

### ZRANK key member
作用：返回成员的升序排名（0-based）。不存在返回 Null。

返回：

- 存在：Integer
- 不存在或 key 不存在：`$-1\r\n`
- 类型不匹配：`-ERR ...\r\n`
### BZMPOP timeout numkeys key [key ...] MIN|MAX [COUNT count]
作用：多 key 弹出（非阻塞/阻塞）。

返回：

- 命中：Array，结构为 `[key, [[member1, score1], ...]]`
- 未命中：Null Array（阻塞命令超时同理）

## 7. Function（Redis 7 推荐替代 EVAL 体系）

### FCALL function numkeys [key ...] [arg ...]
### FCALL_RO function numkeys [key ...] [arg ...]
作用：调用已加载函数（读写/只读）。

返回：由函数返回值决定，可为任意 RESP 类型。

### FUNCTION LOAD [REPLACE] code
### FUNCTION LIST [LIBRARYNAME pattern] [WITHCODE]
### FUNCTION STATS
### FUNCTION DELETE library
### FUNCTION FLUSH [ASYNC|SYNC]
### FUNCTION DUMP
### FUNCTION RESTORE payload [FLUSH|APPEND|REPLACE]
作用：函数库生命周期管理。

返回（常见）：

- `FUNCTION LOAD` / `FUNCTION FLUSH` / `FUNCTION RESTORE`：`+OK\r\n`
- `FUNCTION DELETE`：`+OK\r\n`
- `FUNCTION LIST` / `FUNCTION STATS`：Array
- `FUNCTION DUMP`：Bulk String

## 8. Pub/Sub（含 7.x 分片能力）

### PUBLISH channel message
作用：向频道发布消息。

返回：Integer，收到消息的订阅者数量。

### SUBSCRIBE channel [channel ...]
### PSUBSCRIBE pattern [pattern ...]
作用：订阅频道/模式。

返回：进入订阅流模式，服务端持续推送 Array 消息帧。

### SPUBLISH shard-channel message
### SSUBSCRIBE shard-channel [shard-channel ...]
### SUNSUBSCRIBE [shard-channel ...]
### PUBSUB SHARDCHANNELS [pattern]
### PUBSUB SHARDNUMSUB [shard-channel ...]
作用：分片发布订阅（Redis 7）。

返回（常见）：

- `SPUBLISH`：Integer
- `PUBSUB SHARDCHANNELS`：Array
- `PUBSUB SHARDNUMSUB`：Array（`channel, count, ...`）
- `SSUBSCRIBE` / `SUNSUBSCRIBE`：订阅流消息帧

## 9. 事务与一致性

### MULTI / EXEC / DISCARD / WATCH
作用：事务队列与乐观锁。

返回（常见）：

- `MULTI`：`+OK\r\n`
- 事务中命令入队：`+QUEUED\r\n`
- `EXEC`：Array；若 WATCH 冲突则 Null Array（`*-1\r\n`）
- `DISCARD`：`+OK\r\n`
- `WATCH`：`+OK\r\n`

### WAIT numreplicas timeout
作用：等待复制确认。

返回：Integer，超时前已确认副本数。

### WAITAOF numlocal numreplicas timeout
作用：等待 AOF + 复制确认（Redis 7.2）。

返回：Array（计数结果结构）。

## 10. 集群

### CLUSTER SHARDS
作用：返回分片拓扑视图。

返回：Array（分片与节点信息）。

## 11. 已剔除的不推荐命令（示例）

以下命令在本清单中不再推荐使用：

- `HMSET`：用多字段 `HSET` 替代
- `ZRANGEBYSCORE` / `ZREVRANGEBYSCORE`：用统一 `ZRANGE ... BYSCORE [REV]` 替代
- `GEORADIUS` / `GEORADIUSBYMEMBER`：用 `GEOSEARCH` 替代
- `SETNX`：多数场景用 `SET key value NX` 替代

说明：被剔除不代表 Redis 立刻不可用，而是语义上已有更清晰、更统一的新写法。
