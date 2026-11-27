#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
[[ -f models/massive_ru.bin && -f results/massive/threshold.txt ]] || bash run_massive.sh
mkdir -p bin
[[ -x bin/intent ]] || "${CC:-cc}" -std=c11 -O2 src/intent.c -lm -o bin/intent
threshold="$(cat results/massive/threshold.txt)"
if [[ "$#" -eq 0 ]]; then
 exec bin/intent models/massive_ru.bin chat "$threshold" actions.tsv
else
 exec bin/intent models/massive_ru.bin ask "$threshold" actions.tsv "$*"
fi
