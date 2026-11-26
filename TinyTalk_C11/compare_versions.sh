#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
bash build.sh
question="${1:-какой твой любимый цвет}"
echo "Запрос: $question"
found=0
for path in models/v*/model.bin; do
  [ -f "$path" ] || continue
  found=1
  printf '%s: ' "$path"
  ./bin/tiny ask "$path" "$question"
done
if [ "$found" -eq 0 ]; then echo 'Сохранённых версий пока нет.'; fi
