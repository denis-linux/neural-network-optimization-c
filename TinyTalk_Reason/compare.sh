#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
[[ -f models/intent.bin ]] || bash start_learning.sh
bash build.sh
echo '=== Hybrid ==='
bin/reason models/intent.bin --batch < tests/neural_demo.txt
echo '=== Rules only ==='
bin/reason models/intent.bin --batch --rules-only < tests/neural_demo.txt
