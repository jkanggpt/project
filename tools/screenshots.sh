#!/usr/bin/env bash
# Xvfb(가상 화면)에서 게임을 OpenGL 로 띄워 주요 장면을 PNG 로 저장합니다.
# 결과: artifacts/screenshots/*.png (커밋하지 않습니다. README 용은 골라서 docs/screenshots/ 에 복사)
# 사용법: tools/screenshots.sh [시드=42] [해상도=1280x720]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SEED="${1:-42}"
RESOLUTION="${2:-1280x720}"
GODOT="${GODOT:-godot}"
OUT="$ROOT/artifacts/screenshots"
cd "$ROOT"

cmake --preset linux-release -DCASTLE_BUILD_TESTS=OFF > /dev/null
cmake --build --preset linux-release
dotnet build game/MagicCastle.csproj -nologo -v quiet
timeout 600 "$GODOT" --headless --path game --import > /dev/null 2>&1

mkdir -p "$OUT"
timeout 300 xvfb-run -a -s "-screen 0 1920x1080x24" \
  "$GODOT" --path game --rendering-driver opengl3 --audio-driver Dummy --resolution "$RESOLUTION" \
  -- --screenshots "$OUT" --seed "$SEED"
ls -1 "$OUT"
