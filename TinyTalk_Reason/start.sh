#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
[[ -x bin/reason ]] || bash build.sh
[[ -f models/intent.bin ]] || bash start_learning.sh
exec bin/reason models/intent.bin "$@"
