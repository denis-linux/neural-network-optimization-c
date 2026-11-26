#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
compiler="${CC:-cc}"
if ! command -v "$compiler" >/dev/null 2>&1; then
  echo 'Нужен компилятор C: GCC или Clang. На macOS: xcode-select --install' >&2
  exit 1
fi
mkdir -p bin
"$compiler" -std=c11 -O2 -pedantic tiny.c -lm -o bin/tiny
