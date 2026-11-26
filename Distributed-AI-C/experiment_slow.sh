#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
./build.sh
mkdir -p results
port=${PORT:-19009}
./start_server.sh "$port" 2000 4096 > results/heterogeneous.log & s=$!
trap 'kill "$s" 2>/dev/null || true' EXIT
sleep .2
./start_worker.sh 127.0.0.1 "$port" 0 > results/fast.log & a=$!
./start_worker.sh 127.0.0.1 "$port" 30 > results/slow.log & b=$!
wait "$a" "$b"
cat results/heterogeneous.log results/fast.log results/slow.log
