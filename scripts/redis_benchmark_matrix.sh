#!/usr/bin/env bash

set -euo pipefail

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-26379}"
OUTPUT_DIR="${OUTPUT_DIR:-benchmark-results/$(date +%Y%m%d-%H%M%S)-${PORT}}"
REDIS_BENCHMARK_BIN="${REDIS_BENCHMARK_BIN:-redis-benchmark}"

mkdir -p "$OUTPUT_DIR"

require_bin() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing binary: $1" >&2
    exit 1
  fi
}

require_bin "$REDIS_BENCHMARK_BIN"

bench() {
  local name="$1"
  shift

  echo "== $name =="
  {
    echo "# $name"
    echo "command: $REDIS_BENCHMARK_BIN $*"
    echo
    "$REDIS_BENCHMARK_BIN" "$@"
  } | tee "$OUTPUT_DIR/${name}.txt"
  echo
}

control() {
  "$REDIS_BENCHMARK_BIN" -h "$HOST" -p "$PORT" -c 1 -n 1 -P 1 "$@" >/dev/null
}

seed_strings() {
  local requests="$1"
  local range="$2"
  local payload="${3:-3}"
  "$REDIS_BENCHMARK_BIN" -h "$HOST" -p "$PORT" -n "$requests" -r "$range" -d "$payload" SET "key:__rand_int__" value >/dev/null
}

seed_zsets() {
  local requests="$1"
  local range="$2"
  "$REDIS_BENCHMARK_BIN" -h "$HOST" -p "$PORT" -n "$requests" -r "$range" ZADD "zset:__rand_int__" __rand_int__ "member:__rand_int__" >/dev/null
}

echo "output dir: $OUTPUT_DIR"
echo "target: $HOST:$PORT"
echo

bench baseline_set_get \
  -h "$HOST" -p "$PORT" -c 50 -n 2000000 -P 256 -r 100000 -t set,get

bench payload_16b_set_get \
  -h "$HOST" -p "$PORT" -c 50 -n 1000000 -P 128 -r 100000 -d 16 --csv -t set,get

bench payload_256b_set_get \
  -h "$HOST" -p "$PORT" -c 50 -n 1000000 -P 128 -r 100000 -d 256 --csv -t set,get

bench payload_1k_set_get \
  -h "$HOST" -p "$PORT" -c 50 -n 500000 -P 64 -r 100000 -d 1024 --csv -t set,get

bench payload_4k_set_get \
  -h "$HOST" -p "$PORT" -c 50 -n 300000 -P 32 -r 100000 -d 4096 --csv -t set,get

bench payload_16k_set_get \
  -h "$HOST" -p "$PORT" -c 50 -n 100000 -P 16 -r 100000 -d 16384 --csv -t set,get

bench concurrency_c1_p1_set_get \
  -h "$HOST" -p "$PORT" -c 1 -n 200000 -P 1 -r 100000 -t set,get

bench concurrency_c10_p16_set_get \
  -h "$HOST" -p "$PORT" -c 10 -n 500000 -P 16 -r 100000 -t set,get

bench concurrency_c50_p64_set_get \
  -h "$HOST" -p "$PORT" -c 50 -n 2000000 -P 64 -r 100000 -t set,get

bench concurrency_c100_p256_set_get \
  -h "$HOST" -p "$PORT" -c 100 -n 2000000 -P 256 -r 100000 -t set,get

control FLUSHALL
seed_strings 200000 100000

bench exists \
  -h "$HOST" -p "$PORT" -c 50 -n 1000000 -P 64 -r 100000 EXISTS "key:__rand_int__"

bench type \
  -h "$HOST" -p "$PORT" -c 50 -n 500000 -P 32 -r 100000 TYPE "key:__rand_int__"

bench strlen \
  -h "$HOST" -p "$PORT" -c 50 -n 500000 -P 32 -r 100000 STRLEN "key:__rand_int__"

bench del \
  -h "$HOST" -p "$PORT" -c 50 -n 300000 -P 16 -r 100000 DEL "key:__rand_int__"

control FLUSHALL
seed_strings 200000 100000

bench expire \
  -h "$HOST" -p "$PORT" -c 50 -n 500000 -P 32 -r 100000 EXPIRE "key:__rand_int__" 60

bench ttl \
  -h "$HOST" -p "$PORT" -c 50 -n 500000 -P 32 -r 100000 TTL "key:__rand_int__"

bench persist \
  -h "$HOST" -p "$PORT" -c 50 -n 500000 -P 32 -r 100000 PERSIST "key:__rand_int__"

control FLUSHALL
seed_strings 100000 100000

bench rename \
  -h "$HOST" -p "$PORT" -c 20 -n 100000 -P 8 -r 100000 RENAME "key:__rand_int__" "key2:__rand_int__"

control FLUSHALL

bench zadd \
  -h "$HOST" -p "$PORT" -c 20 -n 300000 -P 16 -r 10000 ZADD "zset:__rand_int__" __rand_int__ "member:__rand_int__"

seed_zsets 100000 10000

bench zscore \
  -h "$HOST" -p "$PORT" -c 20 -n 300000 -P 16 -r 10000 ZSCORE "zset:__rand_int__" "member:__rand_int__"

bench dbsize \
  -h "$HOST" -p "$PORT" -c 1 -n 1000 DBSIZE

bench flushall \
  -h "$HOST" -p "$PORT" -c 1 -n 100 FLUSHALL

bench bgrewriteaof \
  -h "$HOST" -p "$PORT" -c 1 -n 20 BGREWRITEAOF

echo "done. logs saved in $OUTPUT_DIR"