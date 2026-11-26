#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
bash build.sh
version="${1:-$(cat models/latest.txt 2>/dev/null || true)}"
if [ -z "$version" ] || [ ! -f "models/$version/model.bin" ]; then echo 'Сначала: bash start_learning.sh' >&2; exit 1; fi
./bin/tiny chat "models/$version/model.bin"
