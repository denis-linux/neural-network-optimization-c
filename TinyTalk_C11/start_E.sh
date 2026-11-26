#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
bash build.sh
version="${1:-$(cat models/latest.txt 2>/dev/null || true)}"
if [ -z "$version" ] || [ ! -f "models/$version/model.bin" ]; then echo 'Сначала: bash start_learning.sh' >&2; exit 1; fi
mkdir -p reports
tests="$(mktemp "reports/expected.XXXXXX")"
trap 'rm -f "$tests"' EXIT
# Реплика и ожидаемый ответ разделены настоящим TAB.
cat > "$tests" <<'TESTS'
привет	Привет, как дела?
как дела	Всё хорошо, спасибо! А у тебя?
спасибо	Пожалуйста! Рад помочь.
пока	До встречи! Хорошего дня.
как тебя зовут	Меня зовут TinyTalk. Я маленькая обучаемая нейросеть.
поставь таймер	Я могу только ответить текстом. Системные действия здесь не выполняются.
TESTS
status=0
echo "Модель: $version"
./bin/tiny eval "models/$version/model.bin" "$tests" | tee "reports/${version}_smoke.txt" || status=1
./bin/tiny eval "models/$version/model.bin" tests_seen.tsv | tee "reports/${version}_seen.txt" || status=1
# Незнакомые формулировки: результат показывается честно, ошибки возможны.
./bin/tiny eval "models/$version/model.bin" tests_unseen.tsv | tee "reports/${version}_unseen.txt" || status=1
if [ -n "${EXTRA_TESTS:-}" ]; then
  ./bin/tiny eval "models/$version/model.bin" "$EXTRA_TESTS" | tee "reports/${version}_extra.txt" || status=1
fi
echo 'Для диалога: bash chat.sh'
exit "$status"
