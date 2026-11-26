#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
[[ -f models/intent.bin ]] || bash start_learning.sh
mkdir -p bin
"${CC:-cc}" -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer src/reason.c -lm -o bin/reason_san
ASAN_OPTIONS=detect_leaks=0 bin/reason_san models/intent.bin --test tests/dialogues.tsv
