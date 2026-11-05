#!/usr/bin/env bash
# Godot 헤드리스 스모크 테스트 (Linux).
#   C 코어 릴리스 빌드 → C# 빌드 → 에셋 가져오기 → N일 자동 진행.
#   엔진 오류·경고가 하나라도 나오거나 검사가 실패하면 0 이 아닌 값으로 끝납니다.
# 사용법: tools/smoke_test.sh [일수=20]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DAYS="${1:-20}"
GODOT="${GODOT:-godot}"
cd "$ROOT"

cmake --preset linux-release -DCASTLE_BUILD_TESTS=OFF > /dev/null
cmake --build --preset linux-release
dotnet build game/MagicCastle.csproj -nologo -v quiet

# 처음 받은 저장소에는 .godot/ 가 없으므로 에셋을 먼저 가져옵니다
timeout 600 "$GODOT" --headless --path game --import > /dev/null 2>&1

LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT
set +e
timeout 600 "$GODOT" --headless --path game -- --smoke-test --days "$DAYS" --seed 42 2>&1 | tee "$LOG"
STATUS=${PIPESTATUS[0]}
set -e

if grep -qE "^(ERROR|WARNING|SCRIPT ERROR|USER ERROR|USER WARNING)" "$LOG"; then
  echo "스모크 테스트 실패: 출력에 오류나 경고가 있습니다." >&2
  exit 1
fi
if [ "$STATUS" -ne 0 ]; then
  echo "스모크 테스트 실패: 종료 코드 $STATUS" >&2
  exit "$STATUS"
fi
echo "스모크 테스트 통과"
