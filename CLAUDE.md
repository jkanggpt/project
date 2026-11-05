# 마법성채 키우기 — instructions for Claude

## Read first, every session

1. `docs/SPEC.md` — the full build spec: architecture, rules, game design, milestones, quality gates. It takes priority over everything except the students' own messages.
2. `PROGRESS.md` — where the last session stopped and what comes next.

## Every session

- Check the tools before starting work: `cmake --version`, `dotnet --version`, `godot --version`. If one is missing, see SPEC §2.1.
- Work on the branch for the current milestone. Never push to `main` and never merge PRs.
- Commit in small steps and push often.
- Before ending a session: the build is green, all tests pass, `PROGRESS.md` is updated, and everything is committed and pushed.
- Language: Korean for player-facing text, the students' docs, and code comments; English for identifiers.

## Commands

Run from the repository root. Presets live in `CMakePresets.json`.

- Configure + build C (Linux, sanitizers): `cmake --preset linux-debug -DCASTLE_WERROR=ON && cmake --build --preset linux-debug` (Clang: preset `linux-clang-debug`; needs `libclang-rt-18-dev`)
- Run C tests: `ctest --preset linux-debug -j8`
- Soak test (quality gate, ~9 min under ASan): `build/linux-debug/bin/castle_cli --soak 400 --seeds 20`
- Simulator CSV: `mkdir -p artifacts && build/linux-debug/bin/castle_cli --simulate 60 --seed 7 --bot reasonable --out artifacts/sim.csv` (bots: `idle`, `reasonable`; add `--events` to print notifications)
- The CLI reads definition files from `game/data` by default (`--data <dir>` to change). Regenerate them from the built-in defaults: `build/linux-debug/bin/castle_cli --dump-defs game/data`
- Windows cross-compile check (MinGW): `cmake --preset mingw-release -DCASTLE_WERROR=ON && cmake --build --preset mingw-release`
- Release core for Godot (copies `libcastle_core.so` to `game/bin/linux-x64/`): `cmake --preset linux-release -DCASTLE_BUILD_TESTS=OFF && cmake --build --preset linux-release`
- Build C#: `dotnet build game/MagicCastle.csproj` (warnings-as-errors check: `-c ExportRelease`)
- Import assets (needed once per fresh clone): `godot --headless --path game --import`
- Godot headless smoke test (builds core + C#, imports, runs N days): `tools/smoke_test.sh 20`
- Screenshots under Xvfb (to `artifacts/screenshots/`): `tools/screenshots.sh [seed] [WxH]`
- Regenerate all art (terrain, buildings, villagers, icons, roads) from the Kenney packs: `python3 tools/make_art.py`
- Windows (students): `cmake --preset windows-msvc && cmake --build --preset windows-msvc && ctest --preset windows-msvc`
