#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
exec ./trainer server "${1:-9000}" "${2:-2000}" "${3:-4096}" "${4:-127.0.0.1}"
