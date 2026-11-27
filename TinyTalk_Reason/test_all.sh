#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
bash start_E.sh
bash test_properties.sh
python3 tests/test_converter.py
printf '%s\n' 'Core and converter checks passed. For the independent MASSIVE experiment: bash run_massive.sh'
