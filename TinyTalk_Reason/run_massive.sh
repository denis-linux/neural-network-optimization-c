#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
command -v python3 >/dev/null || { echo 'Нужен Python 3 только для подготовки данных и отчёта.'; exit 1; }
mkdir -p results/massive models
python3 tools/fetch_massive.py
python3 tools/convert_dataset.py datasets/ru-RU.jsonl datasets/massive_ru --format massive > results/massive/conversion.json
bash build.sh
"${CC:-cc}" -std=c11 -O2 src/intent.c -lm -o bin/intent
# Separate checkpoint: does not replace the original Reason model.
bin/train train datasets/massive_ru/train.txt models/massive_ru.bin 40 > results/massive/training.log
bin/intent models/massive_ru.bin predict datasets/massive_ru/validation.txt > results/massive/validation_predictions.tsv
bin/intent models/massive_ru.bin predict datasets/massive_ru/test.txt > results/massive/test_predictions.tsv
python3 tools/evaluate_intents.py results/massive datasets/massive_ru | tee results/massive/summary.txt
