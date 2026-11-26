#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
bash build.sh
mode=auto
case "${1:-}" in
  --fresh) mode=fresh; shift ;;
  --resume) mode=resume; shift ;;
esac
if [ "$#" -gt 1 ]; then echo 'Использование: bash start_learning.sh [--fresh|--resume] [extra_data.txt]' >&2; exit 1; fi
extra="${1:-}"
if [ -n "$extra" ] && [ ! -f "$extra" ]; then echo "Нет файла: $extra" >&2; exit 1; fi
mkdir -p models
if ! mkdir models/.training_lock 2>/dev/null; then echo 'Другое обучение уже запущено (models/.training_lock).' >&2; exit 1; fi
trap 'rmdir models/.training_lock' EXIT
previous=''
if [ "$mode" != fresh ] && [ -f models/latest.txt ]; then
  previous="$(cat models/latest.txt)"
  case "$previous" in v[0-9][0-9][0-9][0-9]*) ;; *) echo 'Неверный latest.txt' >&2; exit 1;; esac
  [ -f "models/$previous/model.bin" ] || { echo 'Нет предыдущей модели' >&2; exit 1; }
fi
if [ "$mode" = resume ] && [ -z "$previous" ]; then echo 'Сначала запустите обучение без --resume.' >&2; exit 1; fi
i=1
while :; do
  printf -v tag 'v%04d' "$i"
  if [ ! -e "models/$tag" ]; then break; fi
  i=$((i+1))
done
mkdir "models/$tag"
if [ -n "$previous" ]; then
  cp "models/$previous/training.tsv" "models/$tag/training.tsv"
  printf '\n' >> "models/$tag/training.tsv"
  cat data.txt >> "models/$tag/training.tsv"
else
  cp data.txt "models/$tag/training.tsv"
fi
if [ -n "$extra" ]; then
  printf '\n' >> "models/$tag/training.tsv"
  cat "$extra" >> "models/$tag/training.tsv"
fi
awk '!seen[$0]++' "models/$tag/training.tsv" > "models/$tag/unique.tmp"
mv "models/$tag/unique.tmp" "models/$tag/training.tsv"
awk -F '\t' '
  /^#/ || /^$/ { next }
  NF != 2 { print "Нужны ровно два поля с TAB, строка " NR > "/dev/stderr"; exit 1 }
  ($1 in answer) && answer[$1] != $2 { print "Противоречащие ответы для реплики: " $1 > "/dev/stderr"; exit 1 }
  { answer[$1] = $2 }
' "models/$tag/training.tsv"
epochs="${EPOCHS:-160}"
echo "Версия: $tag; предыдущая: ${previous:-нет, случайные веса}; эпох: $epochs"
if [ -n "$previous" ]; then
  ./bin/tiny train "models/$tag/training.tsv" "models/$tag/model.bin" "$epochs" "models/$previous/model.bin" | tee "models/$tag/train.log"
else
  ./bin/tiny train "models/$tag/training.tsv" "models/$tag/model.bin" "$epochs" | tee "models/$tag/train.log"
fi
printf '%s\n' "${previous:-none}" > "models/$tag/parent.txt"
printf '%s\n' "$tag" > models/latest.tmp
mv models/latest.tmp models/latest.txt
echo "Сохранено models/$tag/model.bin. Проверка: bash start_E.sh $tag"
