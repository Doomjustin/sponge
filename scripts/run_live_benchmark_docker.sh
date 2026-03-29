#!/usr/bin/env bash

set -euo pipefail

: "${SUDO_PASSWORD:?SUDO_PASSWORD is required}"

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-26379}"
OUT="${OUT:-/home/doom/sponge/benchmark-results/relwithdebinfo-live-20260330}"

mkdir -p "$OUT"

rb() {
  printf '%s\n' "$SUDO_PASSWORD" | sudo -S docker run --rm --network host redis:7 redis-benchmark "$@"
}

rc() {
  printf '%s\n' "$SUDO_PASSWORD" | sudo -S docker run --rm --network host redis:7 redis-cli "$@"
}

bench() {
  local name="$1"
  shift
  {
    echo "# $name"
    echo "command: redis-benchmark $*"
    echo
    rb "$@"
  } > "$OUT/$name.txt" 2>&1
}

rc -h "$HOST" -p "$PORT" FLUSHALL >/dev/null 2>&1 || true
bench baseline_set_get -h "$HOST" -p "$PORT" -c 50 -n 2000000 -P 256 -r 100000 -t set,get
bench payload_1k_set_get -h "$HOST" -p "$PORT" -c 50 -n 500000 -P 64 -r 100000 -d 1024 -t set,get
bench payload_16k_set_get -h "$HOST" -p "$PORT" -c 50 -n 100000 -P 16 -r 100000 -d 16384 -t set,get
bench concurrency_c1_p1_set_get -h "$HOST" -p "$PORT" -c 1 -n 200000 -P 1 -r 100000 -t set,get
bench concurrency_c100_p256_set_get -h "$HOST" -p "$PORT" -c 100 -n 2000000 -P 256 -r 100000 -t set,get

rc -h "$HOST" -p "$PORT" FLUSHALL >/dev/null 2>&1 || true
rb -h "$HOST" -p "$PORT" -n 200000 -r 100000 SET key:__rand_int__ value >/dev/null 2>&1
bench exists -h "$HOST" -p "$PORT" -c 50 -n 500000 -P 64 -r 100000 EXISTS key:__rand_int__
bench type -h "$HOST" -p "$PORT" -c 50 -n 300000 -P 32 -r 100000 TYPE key:__rand_int__
bench strlen -h "$HOST" -p "$PORT" -c 50 -n 300000 -P 32 -r 100000 STRLEN key:__rand_int__
bench del -h "$HOST" -p "$PORT" -c 50 -n 200000 -P 16 -r 100000 DEL key:__rand_int__

rc -h "$HOST" -p "$PORT" FLUSHALL >/dev/null 2>&1 || true
rb -h "$HOST" -p "$PORT" -n 200000 -r 100000 SET key:__rand_int__ value >/dev/null 2>&1
bench expire -h "$HOST" -p "$PORT" -c 50 -n 300000 -P 32 -r 100000 EXPIRE key:__rand_int__ 60
bench ttl -h "$HOST" -p "$PORT" -c 50 -n 300000 -P 32 -r 100000 TTL key:__rand_int__
bench persist -h "$HOST" -p "$PORT" -c 50 -n 300000 -P 32 -r 100000 PERSIST key:__rand_int__

rc -h "$HOST" -p "$PORT" FLUSHALL >/dev/null 2>&1 || true
rb -h "$HOST" -p "$PORT" -n 100000 -r 100000 SET key:__rand_int__ value >/dev/null 2>&1
bench rename -h "$HOST" -p "$PORT" -c 20 -n 100000 -P 8 -r 100000 RENAME key:__rand_int__ key2:__rand_int__

rc -h "$HOST" -p "$PORT" FLUSHALL >/dev/null 2>&1 || true
bench zadd -h "$HOST" -p "$PORT" -c 20 -n 200000 -P 16 -r 10000 ZADD zset:__rand_int__ __rand_int__ member:__rand_int__
rb -h "$HOST" -p "$PORT" -n 100000 -r 10000 ZADD zset:__rand_int__ __rand_int__ member:__rand_int__ >/dev/null 2>&1
bench zscore -h "$HOST" -p "$PORT" -c 20 -n 200000 -P 16 -r 10000 ZSCORE zset:__rand_int__ member:__rand_int__

bench dbsize -h "$HOST" -p "$PORT" -c 1 -n 1000 DBSIZE
bench flushall -h "$HOST" -p "$PORT" -c 1 -n 100 FLUSHALL
bench bgrewriteaof -h "$HOST" -p "$PORT" -c 1 -n 20 BGREWRITEAOF

printf 'done\n' > "$OUT/COMPLETE"
echo "RESULT_DIR=$OUT"