#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p bin
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Wno-misleading-indentation src/tiny_base.c -lm -o bin/train
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Wno-misleading-indentation src/reason.c -lm -o bin/reason
