#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
cc -std=c11 -O3 -Wall -Wextra -Wpedantic trainer.c -o trainer -lm -pthread
