#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
bash build.sh
[[ -f models/intent.bin ]] || bash start_learning.sh
bin/reason models/intent.bin --test tests/dialogues.tsv
bin/train eval models/intent.bin tests/intents.tsv
