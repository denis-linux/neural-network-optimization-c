#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
exec ./trainer worker "${1:-127.0.0.1}" "${2:-9000}" "${3:-0}"
