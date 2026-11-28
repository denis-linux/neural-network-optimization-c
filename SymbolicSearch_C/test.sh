#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/symbolic_search_$$"
trap 'rm -f "$out"' EXIT
cc -std=c11 -O2 -Wall -Wextra -pedantic SymbolicSearch_C/search.c -o "$out"
"$out" TinyTalk_Reason/data.txt "поставь таймер"
"$out" TinyTalk_Reason/data.txt "сколько осталось"
"$out" TinyTalk_Reason/data.txt "привет"
