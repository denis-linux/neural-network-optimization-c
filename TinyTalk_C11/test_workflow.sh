#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p checks
temp="$(mktemp -d "$(pwd)/checks/workflow space.XXXXXX")"
trap 'rm -rf "$temp"' EXIT
cp tiny.c ./*.sh data.txt extra_data.txt tests_seen.tsv tests_extra.tsv "$temp/"
bash "$temp/start_learning.sh" --fresh > "$temp/train1.log"
old="$(cksum "$temp/models/v0001/model.bin")"
EPOCHS=60 bash "$temp/start_learning.sh" --resume "$temp/extra_data.txt" > "$temp/train2.log"
EPOCHS=40 bash "$temp/start_learning.sh" --resume > "$temp/train3.log"
EPOCHS=40 bash "$temp/start_learning.sh" --resume > "$temp/train4.log"
[ "$old" = "$(cksum "$temp/models/v0001/model.bin")" ]
for v in v0001 v0002 v0003 v0004; do
  "$temp/bin/tiny" eval "$temp/models/$v/model.bin" "$temp/tests_seen.tsv" > "$temp/$v.log"
done
for v in v0002 v0003 v0004; do
  "$temp/bin/tiny" eval "$temp/models/$v/model.bin" "$temp/tests_extra.tsv" > "$temp/${v}_extra.log"
done
grep -q 'previous_epochs=160' "$temp/train2.log"
grep -q 'previous_epochs=260' "$temp/train4.log"
echo 'PASS: обучение с нуля; v1 → v2 → v3 → v4; новый ответ; старые ответы; неизменность v1; пути с пробелами.'
