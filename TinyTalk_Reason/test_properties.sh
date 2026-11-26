#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
[[ -f models/intent.bin ]] || bash start_learning.sh
mkdir -p bin
"${CC:-cc}" -std=c11 -O2 tests/properties.c -lm -o bin/properties
bin/properties models/intent.bin
