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

<!-- Claude keeps this section current as the project grows. -->

- Configure + build C (Linux, sanitizers): _to be filled in during milestone 1_
- Run C tests: _to be filled in during milestone 1_
- Windows cross-compile check (MinGW): _to be filled in during milestone 1_
- Build C#: _to be filled in during milestone 1_
- Godot headless smoke test: _to be filled in during milestone 1_
- Screenshots under Xvfb: _to be filled in when visuals exist_
