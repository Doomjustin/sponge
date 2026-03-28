# Redis 推荐命令清单（已移除不推荐命令）

本文档保留“当前推荐使用”的 Redis 命令，并剔除不推荐/过时命令（例如 `HMSET`、`ZRANGEBYSCORE` 等可被新语法替代项）。

说明：

- 以 Redis 7.x 常见用法为准。
- 重点说明：命令作用、关键选项、返回语义。

## 1. Key 与过期

### EXISTS key [key ...]
作用：统计存在的 key 数量。

### DEL key [key ...]
作用：删除 key。

### UNLINK key [key ...]
作用：异步删除，适合大 key。

### TYPE key
作用：返回 key 类型（不存在时 `none`）。

### RENAME key newkey
作用：把 `key` 重命名为 `newkey`。

语义：

- 源 key 不存在时返回错误
- 目标 key 已存在时会被覆盖

### RENAMENX key newkey
作用：仅当 `newkey` 不存在时执行重命名。

返回：

- `1`：重命名成功
- `0`：目标 key 已存在，未执行重命名

### EXPIRE key seconds [NX|XX|GT|LT]
### PEXPIRE key milliseconds [NX|XX|GT|LT]
作用：设置过期时间。

选项：

- `NX`：仅当当前无过期时间时设置
- `XX`：仅当当前已有过期时间时设置
- `GT`：仅允许延长
- `LT`：仅允许缩短

### EXPIRETIME key
### PEXPIRETIME key
作用：返回绝对过期时间戳（秒/毫秒）。

### TTL key
### PTTL key
返回语义：

- `> 0`：剩余时间
- `-1`：存在但无过期时间
- `-2`：不存在

### PERSIST key
作用：移除过期时间。

## 2. String

### SET key value [EX seconds|PX milliseconds|EXAT ts|PXAT tsms] [NX|XX] [KEEPTTL] [GET]
作用：设置字符串值。

### GET key
作用：读取字符串值。

### MSET key value [key value ...]
作用：批量设置。

### MGET key [key ...]
作用：批量读取。

### INCR key / DECR key / INCRBY key n / DECRBY key n
作用：整数原子增减。

### APPEND key value
作用：字符串尾部拼接。

### STRLEN key
作用：返回字符串长度。

## 3. Hash

### HSET key field value [field value ...]
作用：设置一个或多个字段。

### HGET key field
作用：读取字段值。

### HMGET key field [field ...]
作用：批量读取字段。

### HINCRBY key field increment
作用：字段整数原子增减。

### HDEL key field [field ...]
作用：删除字段。

### HGETALL key
作用：读取全部字段和值。

## 4. List

### LPUSH key element [element ...]
### RPUSH key element [element ...]
作用：头/尾插入。

### LPOP key [count]
### RPOP key [count]
作用：头/尾弹出。

### LRANGE key start stop
作用：按下标范围读取。

### LLEN key
作用：长度。

### LMPOP numkeys key [key ...] LEFT|RIGHT [COUNT count]
### BLMPOP timeout numkeys key [key ...] LEFT|RIGHT [COUNT count]
作用：多 key 弹出（非阻塞/阻塞）。

## 5. Set

### SADD key member [member ...]
作用：添加成员。

### SREM key member [member ...]
作用：删除成员。

### SMEMBERS key
作用：获取所有成员。

### SISMEMBER key member
作用：成员是否存在。

### SCARD key
作用：成员数。

### SINTERCARD numkeys key [key ...] [LIMIT limit]
作用：求交集基数（不返回具体成员）。

## 6. ZSet

### ZADD key [NX|XX] [CH] [INCR] score member [score member ...]
作用：写入有序集合。

### ZRANGE key start stop [REV] [BYSCORE|BYLEX] [LIMIT offset count] [WITHSCORES]
作用：统一范围查询入口（推荐用它替代旧的 `ZRANGEBYSCORE` 等旧接口）。

### ZREM key member [member ...]
作用：删除成员。

### ZSCORE key member
作用：读取成员分数。

### ZCARD key
作用：成员数量。

### ZMPOP numkeys key [key ...] MIN|MAX [COUNT count]
### BZMPOP timeout numkeys key [key ...] MIN|MAX [COUNT count]
作用：多 key 弹出（非阻塞/阻塞）。

## 7. Function（Redis 7 推荐替代 EVAL 体系）

### FCALL function numkeys [key ...] [arg ...]
### FCALL_RO function numkeys [key ...] [arg ...]
作用：调用已加载函数（读写/只读）。

### FUNCTION LOAD [REPLACE] code
### FUNCTION LIST [LIBRARYNAME pattern] [WITHCODE]
### FUNCTION STATS
### FUNCTION DELETE library
### FUNCTION FLUSH [ASYNC|SYNC]
### FUNCTION DUMP
### FUNCTION RESTORE payload [FLUSH|APPEND|REPLACE]
作用：函数库生命周期管理。

## 8. Pub/Sub（含 7.x 分片能力）

### PUBLISH channel message
### SUBSCRIBE channel [channel ...]
### PSUBSCRIBE pattern [pattern ...]
作用：传统发布订阅。

### SPUBLISH shard-channel message
### SSUBSCRIBE shard-channel [shard-channel ...]
### SUNSUBSCRIBE [shard-channel ...]
### PUBSUB SHARDCHANNELS [pattern]
### PUBSUB SHARDNUMSUB [shard-channel ...]
作用：分片发布订阅（Redis 7）。

## 9. 事务与一致性

### MULTI / EXEC / DISCARD / WATCH
作用：事务队列与乐观锁。

### WAIT numreplicas timeout
作用：等待复制确认。

### WAITAOF numlocal numreplicas timeout
作用：等待 AOF + 复制确认（Redis 7.2）。

## 10. 集群

### CLUSTER SHARDS
作用：推荐的分片拓扑视图（优先于旧 `CLUSTER SLOTS`）。

## 11. 已剔除的不推荐命令（示例）

以下命令在本清单中不再推荐使用：

- `HMSET`：用多字段 `HSET` 替代
- `ZRANGEBYSCORE` / `ZREVRANGEBYSCORE`：用统一 `ZRANGE ... BYSCORE [REV]` 替代
- `GEORADIUS` / `GEORADIUSBYMEMBER`：用 `GEOSEARCH` 替代
- `SETNX`：多数场景用 `SET key value NX` 替代

说明：被剔除不代表 Redis 立刻不可用，而是语义上已有更清晰、更统一的新写法。
