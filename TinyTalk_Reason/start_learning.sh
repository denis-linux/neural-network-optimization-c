#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
bash build.sh
mkdir -p models
if [[ "$#" -gt 0 && "$1" != "--resume" ]]; then echo 'Использование: bash start_learning.sh [--resume]'; exit 2; fi
# Atomic replacement; retain the previous checkpoint for comparison.
if [[ "${1:-}" == "--resume" ]]; then
  [[ -f models/intent.bin ]] || { echo 'Сначала выполните обучение с нуля'; exit 1; }
  bin/train train data.txt models/intent.next "${EPOCHS:-100}" models/intent.bin
else
  bin/train train data.txt models/intent.next "${EPOCHS:-220}"
fi
if [[ -f models/intent.bin ]]; then cp models/intent.bin models/intent.previous.bin; fi
mv models/intent.next models/intent.bin
