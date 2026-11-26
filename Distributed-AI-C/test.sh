#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
./build.sh
mkdir -p results
steps=${STEPS:-2000}; batch=${BATCH:-4096}; port=${PORT:-19000}
./trainer local "$steps" "$batch" > results/local.log
for workers in 1 2 4; do
  ./start_server.sh "$port" "$steps" "$batch" > "results/server_${workers}.log" & server=$!
  trap 'kill "$server" 2>/dev/null || true' EXIT
  sleep 0.2
  pids=()
  for ((i=0;i<workers;i++)); do ./start_worker.sh 127.0.0.1 "$port" > "results/worker_${workers}_${i}.log" & pids+=("$!"); done
  for p in "${pids[@]}"; do wait "$p"; done
  kill "$server"; wait "$server" || true; trap - EXIT
  cp model.txt "results/model_${workers}.txt"
  tail -1 "results/server_${workers}.log"
  port=$((port+1))
done
