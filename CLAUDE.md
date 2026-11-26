# 마법성채 키우기 — 개발 안내

먼저 docs/SPEC.md와 PROGRESS.md를 읽습니다. 구조는 docs/ARCHITECTURE.md, 빌드·검사는 docs/BUILD_AND_TEST.md, 실제 검증과 한계는 docs/CLEANUP_AUDIT.md 및 docs/MISSING_FEATURES.md를 따릅니다.

현재 게임은 C11 + raylib이며 그래픽과 콘솔이 같은 GameSession을 사용합니다. Godot/.NET은 필요하지 않습니다. 공개 헤더는 core/include 한 벌이고 실행 에셋은 game/assets/assets_manifest.json이 정본입니다.

마일스톤 브랜치에서 커밋하고 사용자 지시에 따라 해당 브랜치에 푸시합니다. main 푸시나 PR 병합은 하지 않습니다. 한국어 UI·문서·주석과 영어 식별자를 사용합니다.

~~~powershell
cmake --preset windows-msvc -DCASTLE_WERROR=ON
cmake --build --preset windows-msvc --parallel 4
ctest --preset windows-msvc --parallel 4
powershell -File tools/audit_project.ps1 -Output build/reports/audit
~~~

MinGW의 한글 경로 지원은 tools/build_windows.ps1을 사용합니다. 아트 재생성과 독립 설치 절차는 docs/BUILD_AND_TEST.md에 있습니다. 검사 산출물은 build/reports에만 작성하고 사용자 저장을 정리 대상으로 삼지 않습니다.
