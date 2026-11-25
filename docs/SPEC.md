# Build prompt — 마법성채 키우기 (complete game)

> **2026-11-25 v1.2.3.** 최신 기준은 [V1_2_3_SPEC.md](V1_2_3_SPEC.md)와 사용자의 후속 지시입니다. 새 지도160×160·20호수·유한 점토·신규4시설·상인/어선·급수/의료/수렵/추방·저장v11/ABI13을 적용합니다. 기존v9/v10 지도/원본은 보존합니다. 어업 업무복 없이 기존 주민을 배에 합성하고, 상인은 청색·적색 화려한 천을 사용합니다. 건설 직종 버튼은 기본색, 개별 카드만 해금 등급별18% 배경/원색 테두리이며 최신 아틀라스를 공유합니다. 지상 바위만 줄이고 매장량은 유지합니다. 해석은 V1_2_3_IMPLEMENTATION.md, 검사와 미달 성장 목표는 V1_2_3_QA.md를 따릅니다. 기존 식량1/일·발견부터48시간 습격 등 후속 규칙을 유지하며 로컬 커밋만 합니다.

> **2026-11-23 후속 보완.** 사용자의 최신 요청에 따라 최초 발견/첫 공격부터48시간 습격, 즉시 철수와 시야 밖 종료를 적용합니다. 접근 중 건설 허용·농장 여백 보정·프로필/숫자스탯·최초 근무조 선택·세력 경보/이미지 VFX·알림 겹침 수정은 [V1_2_2_PATCH1.md](V1_2_2_PATCH1.md)를 따릅니다. 아래72시간 기록보다 우선합니다.

> **2026-11-22 v1.2.2.** 최신 기준은 [V1_2_2_SPEC.md](V1_2_2_SPEC.md)와 사용자의 추가 요청입니다. 구현 해석은 V1_2_2_IMPLEMENTATION.md, 검증 범위와 한계는 V1_2_2_QA.md를 따릅니다. C11/raylib·48외형/4업무복·실제 일정/정찰/교대·독립 돌층·72시간 습격/실제 철수·저장v10/ABI12를 적용합니다. 기존v9 원본은 보존합니다. 아래 이전 버전 기록보다 최신 사용자 명세를 우선하며 로컬 커밋만 합니다.

> **2026-11-20 v1.2.1 적용.** 최신 기준은 `docs/V1_2_1_SPEC.md`와 학생의 추가 요청입니다. 구현 해석은 `V1_2_1_IMPLEMENTATION.md`, 실제 결과는 `V1_2_1_QA.md`입니다. C11/raylib·실제 노동7.5·30동물·6세력28적·성별 이름324개/12외형·연결 도로·계절 농장·맞춤 성문을 적용합니다. 저장v9/ABI11이며 기존 저장을 삭제하지 않습니다. 로컬 커밋만 사용합니다.

> **2026-11-19 v1.1.1 통합 수정 명세 적용.** 최신 기준은 `docs/V1_1_1_SPEC.md`이며 `V1_1_REFERENCE.md`는 참고 자료입니다.
> 4×4 비회전 영주관, 행복도/기분 분리, 이주/인구 하한, 저장 허브, 10생물군계·수역 온도, 야생20종/가축4종, 시세를 적용했습니다.
> 저장 v8 / ABI10. 기존 저장은 그대로 보관하고 기본 저장 경로를 `saves_v8`로 분리했습니다.
> 추가 요청의 도로·건물 비율·이주 시야·수목20개·표시 순서·물가 연결은 `V1_1_1_IMPLEMENTATION.md`, 실제 검증 결과는 `V1_1_1_QA.md`를 참고합니다.
> 아래 이전 버전 기록과 충돌하면 v1.1.1과 학생의 최신 요청을 우선합니다. 학생 지시에 따라 GitHub 푸시 없이 로컬 커밋으로 마무리합니다.

> **2026-11-17 통합 확장 기획서 v1.0을 적용했습니다.**
> 최신 규칙은 `docs/expansion/BALANCE_V1_TEXT.md`와 `IMPLEMENTATION.md`를 참고합니다.
> C/raylib 새 게임은 192지도·15자원·건설64종+시작 성채·6등급·연구24종·12일 계절/48일 연도이며 저장은 v7/ABI9입니다.
> 건물 65종×4방향 PNG, 밭 전용 농장, 실내 출입, 무인/욕구/작업 말풍선을 사용합니다.
> 아래 Godot 원문의 이전 수치와 충돌하면 학생의 확장 기획서 및 최신 요청을 우선합니다.

> **2026-11-14 학생 요청으로 기술 구성을 C11 + raylib 5.5로 변경했습니다.**
> 현재 구현·빌드·검사 기준은 `docs/NATIVE_PORT.md`, `docs/ARCHITECTURE.md`, `README.md`입니다.
> 아래 원문의 Godot/C#/.NET·P/Invoke·장면·Python 도구 지시는 전환 이전 설계 기록입니다.
> 게임 규칙·C 과제 요구사항·한국어·라이선스·메모리/저장 안정성 기준은 계속 적용합니다.
>
> **2026-11-16 학생 요청으로 현재 그래픽을 새 중세 판타지 픽셀아트로 교체했습니다.**
> 논리 타일·점유 규칙은 유지하고 주민은 1.5타일 높이, 건물·나무는 뒤의 칸을 가립니다.
> 현재 원본·프롬프트·C 패킹·라이선스·그리기 기준은 `art/README.md`입니다.
> 후속 요청으로 건물·소품은 약 45도 내려다보기, 주민은 직업과 무관한 남녀 8외형과 4방향 걷기로 변경했습니다.

## 0. Mission

Build **마법성채 키우기**, a complete, polished, stable 2D medieval-fantasy castle-management game.

The player is the new lord of a ruined hill keep. They grow it from a 촌락 into a 왕성 by placing buildings, managing named villagers, balancing two currencies (금화 and 마정석), researching magic, and defending against bandits, rival armies, monsters, and nature.

Quality bar: someone playing for 30+ minutes sees no crash, no softlock, no visual glitch, always understands why things happen, and feels a satisfying arc from first farm to royal castle.

Work autonomously. Don't ask me questions unless you are truly blocked. Record design decisions in `docs/DECISIONS.md`. Keep `PROGRESS.md` current so a later session can resume exactly where you stopped.

You are running in a **Claude Code cloud session on a GitHub repository**. §2.1 explains what that means for building, testing, assets and commits.

---

## 1. Hard constraints

### 1.1 The C part is graded

This is a school project (고급프로그래밍). The C code must genuinely use every element below **through real gameplay, not token examples**:

| Requirement | Detail |
| --- | --- |
| Structs | 2+ structs with different purposes; containment and reference relationships between them |
| Pointers | functions that receive pointers and modify the original; array/struct access through pointers |
| Functions | 10+ user-defined functions (excluding `main`), each with one clear role |
| Function pointers | declared, bound to real functions, and used to choose which behavior runs |
| Dynamic memory | `malloc`/`calloc`/`realloc` for data whose size is unknown in advance; every allocation freed |
| File I/O | game data saved and loaded; works after closing and restarting the program |
| Error handling | file-open failure; invalid user input with re-prompt; `NULL` from allocation |

Two students must be able to explain every C file in a Q&A with their teacher. Write C that is clear before it is clever, and produce the docs in §10.

### 1.2 Other constraints

- All art, audio and fonts must be CC0 or SIL OFL. List every asset with source and license in `CREDITS.md`.
- Log every file you generate or substantially edit in `docs/AI_USAGE.md` (the course requires crediting generative AI).
- Final submission is due 2026-12-02. Follow the milestone priorities in §9.

---

## 2. Tech stack (decided)

- **Godot 4.7.2 .NET edition** (pin this exact version so the cloud VM, CI and both students' laptops match), C# with nullable reference types enabled.
- **Compatibility renderer** (OpenGL). It suits 2D pixel art, runs on school laptops' integrated GPUs, and works under software rendering for headless screenshots (§2.1).
- **C11 shared library** `castle_core`, called from C# through P/Invoke.
- **CMake ≥ 3.20.** Windows 64-bit is the primary target and must build with MSVC and MinGW-w64. Keep the code portable to Linux and macOS.
- No other runtime dependencies. The C core uses only the standard library, plus a thin platform wrapper for Windows file APIs (§6.6).

### 2.1 Where you are working: a cloud session

You run on an Ubuntu 24.04 x86_64 VM with **no display**, working on a fresh clone of the repository. The two students playtest on Windows laptops after each milestone. You can't play the game yourself, so the checks below stand in for that.

**Tools.** GCC, Clang, CMake and Ninja are pre-installed. The environment's setup script installs the .NET 8 SDK, MinGW-w64 (x86_64), Xvfb and Mesa, and *tries* to install Godot 4.7.2 .NET at `/opt/godot` with `godot` on `PATH`. At the start of every session, check `cmake --version`, `dotnet --version` and `godot --version`. If Godot is missing, try downloading it yourself. If the network blocks it, keep going: `dotnet build` still compiles all C#. Note the gap in `PROGRESS.md`.

**Building and testing C.** Build and test natively on Linux with GCC and Clang. AddressSanitizer, UBSan and LeakSanitizer all work here, so use them. Also cross-compile the core and CLI with MinGW (`cmake/mingw-w64.cmake` toolchain file) to catch Windows-only compile errors in the platform layer.

**Windows CI.** You can't run Windows here, so GitHub Actions does it. Add `.github/workflows/ci.yml` in milestone 1 with these jobs:
- Linux: GCC and Clang, all tests, sanitizer build, a short soak test;
- Windows (`windows-latest`, MSVC): build and run all C tests;
- C#: `dotnet build` of the Godot project.

Every PR must be green. Read CI results through `gh api` REST calls (e.g. `gh api repos/{owner}/{repo}/actions/runs`); `gh pr` and `gh issue` don't work in this environment.

**Godot checks without a screen.**
- Headless smoke test: build the C# solution, then run a smoke mode (e.g. `godot --headless --path game -- --smoke-test`) that loads the main scene, runs the autopilot for N in-game days, and exits non-zero on any error.
- Screenshots: run the same autopilot under `xvfb-run` with the OpenGL driver, saving PNGs to `artifacts/screenshots/` at key moments (title, early colony, raid, research panel, winter). Open the PNGs and check layout, Korean text rendering, clipping and art. Don't commit them, except a few chosen for the README under `docs/screenshots/`.
- If software rendering doesn't work in this VM, say so in `PROGRESS.md` and rely on the students' playtest.

**Assets.** `kenney.nl` isn't reachable from this environment.
- The students commit the Kenney packs themselves under `game/assets/third_party/kenney/<pack>/`, license files included. Use what's there.
- Galmuri is published as the npm package `galmuri`, and the npm registry is reachable. Get the font files with `npm pack galmuri`, then copy the TTF you need into `game/assets/fonts/`.
- If an expected asset is missing, use procedural placeholders and list exactly what's missing in `PROGRESS.md`, so the students can add it.

**Git workflow.**
- One branch per milestone (`m1-foundations`, `m2-colony-loop`, …). Never push to `main`, and never merge PRs yourself.
- Commit in small, clearly described steps and push often, so work survives if the VM is reclaimed.
- Open a PR when the milestone's checks pass. The PR body has: what was built; a **Windows playtest checklist** for the students (pull, build the DLL, open in Godot, what to try, what should happen); and known issues.
- The next session starts from `main` after the students merge.

**Long commands.** Run long builds and soak tests in the background and poll them, rather than blocking on one command.

---

## 3. Architecture

**Principle: C owns the truth about the colony. Godot owns space and the senses.**

| C core (`castle_core`) | Godot (C#) |
| --- | --- |
| **All persistent game state:** villagers (identity, stats, traits, needs, mood, health, job, home, workplace, current intent), buildings (type, tile position, footprint, level, HP, construction progress, workers, residents), resources and the daily ledger, the static map (terrain grid, occupancy and path-cost grid), season and weather, research, market prices, threat schedule, rank, RNG state | **Everything spatial and real-time:** continuous positions of moving units, pathfinding (`AStarGrid2D` built from C's cost grid), enemy units and combat resolution, projectiles, camera, input, rendering, animation, lighting, particles, audio, UI, menus |
| **All rules:** production, consumption, wages, storage, mood, illness, immigration and departure, villager decisions, placement validation, costs, research effects, event scheduling, raid composition and consequences, rank requirements, win/lose. **All balance numbers**, including enemy and tower stats | **Reports physical facts back to C:** villager arrived / unreachable, damage to buildings and villagers, enemies killed, loot taken, raid ended |
| Save/load of everything above | Only the settings file (volume, resolution, UI scale) |

The static map is data in C because placement rules, productivity (e.g. a lumber camp counts forest tiles in range) and saves all need it. Moving things are not saved.

### 3.1 Boundary rules (these are what make it stable)

1. **Godot never changes game state directly.** Every change goes through a C command or report, and C validates every call.
2. **Opaque handle, flat views.** Godot holds the world only as an opaque pointer. Data crosses as flat view structs (`int32_t` fields, fixed-size UTF-8 char arrays) copied into caller-provided buffers. Godot never keeps pointers into C memory.
3. **Reconcile from snapshots, not events.** After each tick, Godot copies the full entity views (a few hundred entities is cheap) and makes the scene match: create, update, or remove nodes by id. A separate event queue exists only for feedback (sounds, popups, notifications). Missing an event must never desync anything.
4. **Ids are never reused** within a world (monotonic 32-bit counters, saved).
5. **The core is language-free.** C emits event codes with numeric parameters; Godot turns them into Korean text from one central string table. The C CLI has its own small formatter. Villager names are data and may be Korean UTF-8.
6. **Fixed timestep, deterministic core.** One tick = 10 in-game minutes (144 per day). Godot accumulates real time × game speed and calls `core_tick(world, n)`; it never passes a variable `dt` into C. C has its own seeded PRNG (e.g. PCG32) stored in the world and saved. Same seed + same sequence of commands and reports → same state.
7. **Single thread.** Call the core only from Godot's main thread. Never from `[Tool]` scripts or editor plugins; the editor process must not load the DLL, or rebuilding it will fail on a locked file.
8. **ABI check at startup:** Godot compares `core_abi_version()` and the struct sizes reported by C with `Marshal.SizeOf<T>()` and refuses to start with a clear message on mismatch.
9. **Fail gracefully.** Every C API function validates its arguments and returns an error code instead of crashing. Godot wraps every native call in one bridge class that turns codes into typed results and toasts. Exceptions in C# callbacks are caught and logged, never fatal.

### 3.2 Villager handshake

1. Each tick, C runs a per-villager state machine (**function-pointer table indexed by state**): `SLEEP` (home, night), `GO_WORK`, `WORKING`, `EAT`, `IDLE` (town square), `SEEK_HEALING`, `FLEE` (to the keep during raids), `GUARD_POST`, `LEAVING` (walk to map edge, then removed).
2. Each villager exposes an **intent**: state, target building id or target tile, and an `intentRev` that increments whenever the intent changes.
3. Godot paths to the target and animates the walk. On arrival it calls `core_villager_arrived(world, id, intentRev)`. C ignores stale revisions, which safely handles intents that change mid-walk.
4. If no path exists, Godot calls `core_villager_unreachable(world, id, intentRev)`. C picks a fallback and raises a notification ("길이 막혔습니다").
5. Production accrues only while `WORKING`. **Travel time therefore costs work time**, so placing houses near workplaces and building roads matters. This is the game's core spatial strategy.
6. After loading a save, villagers spawn at their current target building or home.

### 3.3 Raid handshake

1. C schedules threats in advance and emits `THREAT_SCHEDULED` (type, direction N/E/S/W, ETA, strength). The UI shows a countdown, so threats are always fair.
2. On arrival, C creates a raid (id, enemy types × counts, scaled by day, wealth and difficulty). Enemy definitions (HP, damage, speed, range, behavior) come from C.
3. Godot spawns the units at the map edge and runs combat.
4. Godot reports `core_report_building_damage`, `core_report_villager_damage`, `core_report_enemy_killed`, `core_report_loot` (enemies reached a storage building) and `core_report_raid_ended`.
5. C applies HP changes, deaths (removed villager; mourning mood penalty), ruined buildings (repairable), rewards (마정석 from monsters), and safety mood.
6. **Combat stays simple and robust:** no physics bodies. Units follow `AStarGrid2D` paths. Walls are high-cost passable cells: when an enemy's next cell is a wall or closed gate, it stops and attacks it. Towers and guards target the nearest enemy in range on a cooldown. Projectiles are visual tweens; damage resolves when they land.
7. **Saving is blocked during a raid.** The save button is disabled with a tooltip, and autosave waits until the raid ends.

---

## 4. Game design

### 4.1 Pillars

1. **Every villager is a person:** named, with traits and visible daily routines. Their small stories surface as notifications ("에드윈이 첫 수확을 거뒀습니다").
2. **Space matters:** distance, terrain adjacency, roads, walls and chokepoints.
3. **Two currencies, two horizons:** 금화 runs today's economy (building, wages, trade). 마정석 is scarce power for the future (research, magic buildings, rank-ups).
4. **Threats are fair:** always telegraphed with time to prepare.
5. **Readable and juicy:** every number has a tooltip explaining it; every action has feedback.

### 4.2 Loops

- **Moment to moment:** watch villagers, place and connect buildings, adjust assignments, respond to alerts.
- **Daily:** production → consumption → wages → mood → events. The ledger shows where everything went.
- **Seasonal:** 4 seasons × 8 days. Winter stops farms and doubles wood use for heating, so autumn is for stockpiling.
- **Whole run:** 촌락 → 마을 → 요새 → 성 → 왕성. Reaching 왕성 wins, with an option to continue endlessly. The game is lost if the keep is destroyed or the population reaches 0.
- **Difficulty presets:** 평화 / 보통 / 혹독, scaling raid strength and frequency and event severity.

### 4.3 Content targets for v1

Treat all numbers as starting points and tune them with the simulator (§8).

- **Resources:** 금화, 마정석, 식량, 목재, 석재, 약초. Storage capacity comes from the keep and 창고.
- **Terrain** (seeded generation, 96×96 tiles, keep in the centre): grass, forest, rock, water (impassable), mana veins. Guarantee fair starts: forest, rock and a mana vein within reach of the keep.
- **Buildings (~15):** 성채 (pre-placed keep; storage; lose condition), 주택 (4 residents), 농장 (seasonal food), 채집 오두막 (herbs + some food; needs nearby forest), 벌목장 (wood ∝ forest tiles in range), 채석장 (needs adjacent rock), 마정석 광산 (on a mana vein), 창고, 시장 (trade), 병영 (guards), 감시탑 (ranged defense), 성벽 and 성문 (drag-placed lines; the gate lets villagers through and stops enemies), 치료소 (heals with herbs), 마탑 (research by mages), 도로 (cheap; faster walking).
- **Construction:** cost paid upfront, then build time in ticks with an under-construction sprite. Demolish refunds 50%. Ruined buildings can be repaired.
- **Villagers:** name (from a names file), age, 4 stats (힘·지능·손재주·체력), 2 traits from ~10 (e.g. 근면함, 게으름, 대식가, 겁쟁이, 용감함, 마력 친화, 낙천가, 투덜이), needs (배고픔, 휴식, 안전, 주거), mood 0–100 that scales work speed. Villagers leave after 2 days below a mood threshold. Immigrants arrive when mood is high and housing is free.
- **Jobs:** buildings have worker slots. Auto-assignment picks the best-suited idle villager; the player can override manually.
- **Nature events:** 가뭄 (summer, farms −50%), 역병 (villagers fall sick; healed at 치료소 with herbs), 폭풍 (building damage), 혹한 (extra winter wood use), 풍년 (good harvest).
- **Threats:** 도적 (small, steal gold), 적대 영주 군대 (large, long warning, later game), 마물 (at night; drop 마정석).
- **Economy:** market prices drift with supply and demand, with a buy/sell spread. A 떠돌이 상인 event offers deals, including 마정석.
- **Research:** ~12 nodes in 3 branches (경제 / 방어 / 마법), costing 금화 + 마정석 and generated by mages at the 마탑. Effects are applied through a function-pointer table.
- **Rank-ups:** requirements (population, average mood, defense, research count) plus a 마정석 tribute. Each rank unlocks buildings.
- **Tutorial:** an objective checklist for the first ~10 minutes (build a farm, build a house, assign a worker, build a wall, survive the first small bandit raid).

---

## 5. Presentation and feel

- **Style:** top-down orthogonal, 16×16 pixel art at integer zoom levels with nearest filtering. Base art: Kenney **Tiny Town** and **Tiny Dungeon** (CC0, 16×16), committed by the students (§2.1). Korean UI font: **Galmuri** (SIL OFL), fetched through npm (§2.1). Where an asset is missing, build clean procedural placeholders and wire everything so swapping in real assets touches only the asset files.
- **World life:** day/night through a `CanvasModulate` gradient, seasonal palette (snow overlay in winter), weather particles (rain, snow), smoke from working buildings.
- **Villager readability:** small bobbing walk, job tool icon, mood/status bubbles drawn as sprites (sleep, hungry, angry, sick), selection outline.
- **Feedback:** floating "+3 식량" from production events, construction dust, placement squash tween, hit flashes, a light screen shake when a wall breaks, a raid warning horn, toasts with icons. Pool floating text and projectiles.
- **HUD:** top bar with the 6 resources and their daily delta (tooltip breakdown from the C ledger), day / clock / season, speed controls (pause, 1×, 2×, 3×), threat countdown; bottom build bar by category with tooltips (cost, requirements, and why a building is disabled); building and villager inspectors; a sortable villager list; research tree; market; a notification feed (click to jump the camera); placement overlays (work radius, terrain productivity preview, invalid tiles).
- **Menus:** title (새 게임: castle name, seed, difficulty / 이어하기 / 불러오기 with 3 slots / 설정 / 종료), pause menu, settings (master, music and SFX volume; UI scale; fullscreen; edge scrolling), victory and defeat screens with a stats summary.
- **Audio:** Kenney CC0 audio packs (committed by the students, §2.1) for UI clicks, construction, combat and alerts. Music only if you find CC0 tracks; otherwise leave a documented slot.
- **Performance:** 60 fps with 150 villagers and 40 enemies on an integrated-GPU laptop; one C tick under 1 ms at 150 villagers.

---

## 6. C core design

Use English identifiers with module prefixes (`world_`, `vil_`, `bld_`, `eco_`, …) and Korean comments that explain *why*. Keep all state inside the world struct; the only globals are `const` tables. The core never calls `printf` or reads stdin.

### 6.1 Suggested modules (refine if you have a better split)

`api` (public API, glue only) · `world` (create/destroy, tick driver) · `grid` (terrain, occupancy, path-cost grid, generation) · `entity` (villager and building pools, id lookup) · `villager_ai` (state handlers) · `jobs` (per-job work functions) · `economy` (production, consumption, wages, storage, ledger, market) · `nature` (season, weather, event handlers) · `threat` (scheduling, raids, consequences) · `research` · `rank` · `defs` (data-file loading) · `save` · `rng` · `events` (feedback queue) · `validate` (invariant checker) · `platform` (UTF-8 file wrapper, atomic replace) · `cli/` (console front end).

### 6.2 Data relationships the graders should see

- A villager **contains** `Stats` and `Needs` structs and **references** `Building *home` and `Building *workplace`.
- A building **references** its villagers through dynamically grown `Villager **workers` and `Villager **residents` arrays.
- The world **contains** a `ResourceBank` and owns pointer arrays of entities that grow with `realloc` (each entity allocated individually, so growing the array never moves an entity).
- Feedback events carry a payload **union** tagged by event type.
- Deleting an entity unlinks both directions before `free`. Loops that remove while iterating must be safe (e.g. iterate backwards with swap-remove).

### 6.3 Function pointers with real jobs

- villager state handlers (`STATE_TABLE[state]`)
- per-job work functions (`JOB_TABLE[job]`)
- nature event handlers
- research effect functions
- data-file section parsers (`{ "building", parse_building }, …`)
- the CLI command table

### 6.4 Dynamic memory with real jobs

Entity pools, terrain and cost grids (`width × height`, allocated at world creation), definition arrays loaded from files (count unknown → grow), the names list, and raid composition arrays. A failed allocation returns an error code and leaves existing state intact (grow into a temporary pointer). Everything is freed in `world_destroy`; ASan/LeakSanitizer must report zero leaks.

### 6.5 Files

- **Definition files** in `data/*.def` (buildings, enemies, research, traits; plain text, one section per entry). Parse errors report file and line number. If a file is missing or broken, fall back to built-in defaults and emit a warning event. Godot reads every definition through the C API, so balance lives in exactly one place.
- **Names file** `data/names.txt` (UTF-8, ~150 medieval-fantasy Korean names).
- **Saves:** versioned text format with a header line (`MAGICCASTLE_SAVE <version>`) and sections. Write ids, never pointers, and relink on load. Include RNG state and id counters. Write atomically (write a temp file, then replace) and keep the previous save as `.bak`. On load, validate every field; on failure, free everything partially loaded, return an error code, and let the UI offer the `.bak`.

### 6.6 Windows pitfalls to handle

You can't run Windows in the cloud VM. These paths are checked by the MinGW cross-compile and the Windows CI job (§2.1); add a CI test that saves and loads through a path containing Korean characters.

- Godot's `user://` folder sits under the Windows user folder, which may contain Korean characters. Plain `fopen` with a UTF-8 path fails there. Wrap file opening in `platform_fopen_utf8()` that uses `_wfopen` on Windows.
- `rename()` fails on Windows when the target exists. Use `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING)` inside the platform wrapper.
- The CLI sets the console to UTF-8 (`SetConsoleOutputCP(CP_UTF8)`, `SetConsoleCP(CP_UTF8)`).
- Build 64-bit only; a 32-bit DLL fails to load in Godot with a confusing error.

### 6.7 Public API (sketch — finalize it yourself)

- **Lifecycle:** `core_create(params)` (castle name, seed, difficulty), `core_destroy`, `core_save(path)`, `core_load(path, &err)`, `core_abi_version`, `core_get_struct_sizes`.
- **Time:** `core_tick(world, n)`.
- **Read:** world summary; ledger; map size; copy terrain grid; copy path-cost grid with a revision number (Godot rebuilds pathing only when it changes); copy all villager views; copy all building views; definitions (buildings, enemies, research, traits); active threats and raids.
- **Placement preview:** `core_can_place(type, x, y)` returning an error code plus a productivity preview (e.g. forest tiles in range).
- **Commands:** place building, place wall line, demolish, repair, assign / unassign worker, toggle auto-assign, trade, unlock research, rank up.
- **Reports:** villager arrived / unreachable, building damage, villager damage, enemy killed, loot, raid ended.
- **Feedback:** `core_poll_event`.
- **Debug:** `core_validate` (returns the number of invariant violations), cheat commands behind a debug flag.

### 6.8 CLI front end (`cli/`)

A text front end that plays the real game through the public API, for the teacher demo and for testing:

- a menu built from a function-pointer command table, with `read_int`-style input validation and re-prompting;
- status, build, assign, trade, research and save/load commands; raids are resolved by a simple numeric battle formula in the CLI, reported through the same report API;
- `--simulate <days> --seed <n> --bot <name>`: a scripted "reasonable player" bot that writes a per-day CSV (resources, population, mood, threats) for balancing;
- `--soak <days> --seeds <n>`: random commands and synthetic reports, running `core_validate` every tick.

---

## 7. Godot design

- **Autoload `Core`:** the bridge. Sets `NativeLibrary.SetDllImportResolver` (editor: `res://bin/<platform>/`; exported build: the executable's folder), runs the ABI check, wraps every call, converts error codes to results.
- **Autoload `GameClock`:** fixed-tick accumulator and speeds. Pausing pauses the scene tree, while UI keeps processing.
- **`WorldSync`:** after each tick, copies views and reconciles villager, building and enemy nodes by id.
- **Map:** `TileMapLayer`s for ground, decoration and roads, generated from C's terrain grid. Buildings are scenes.
- **Pathing:** one `AStarGrid2D` built from C's cost grid and rebuilt only when the grid revision changes. Roads lower the cost; water is solid; walls are high-cost (see §3.3).
- **Agents:** `VillagerAgent` and `EnemyAgent` scenes follow paths at a speed scaled by game speed.
- **Systems:** `CombatDirector`, `BuildMode` (ghost, drag-placement for walls and roads, `core_can_place` preview), `Selection`, `CameraRig` (WASD, edge scroll, middle-drag pan, integer zoom, map bounds), `Notifications`, `Audio` (buses: Master, Music, SFX, UI).
- **Text:** all Korean strings in one table (Godot translation CSV or one static class); no Korean literals scattered through scripts.
- **Settings:** `ConfigFile` in `user://settings.cfg`. Saves: `user://saves/slot{1..3}.sav` plus `autosave.sav`.
- **C# rules:** nullable on; treat warnings as errors in release builds; no allocations in per-frame hot paths; no game rules or balance numbers in C#.

---

## 8. Quality gates

**C**

- Zero warnings with `-Wall -Wextra -Wpedantic` (GCC/Clang) and `/W4` (MSVC).
- Unit tests under CTest: entity create/remove with both link directions; placement rules; production, consumption and wages; the villager handshake including stale `intentRev`; raid consequences; definition-file errors; save → load round trip producing an identical state hash.
- Loader fuzzing: truncated, garbled and wrong-version saves must return errors, never crash.
- AddressSanitizer + UBSan build option; soak test of 400 days × 20 seeds with zero invariant violations and zero leaks.
- **Balance targets on 보통** for the simulator bot: 마을 around day 8, 요새 around day 20, 왕성 around day 50, and at least one tense raid per season after the first year. Tune via `data/*.def`.

**Godot**

- No errors or warnings in the Output panel at startup or during play.
- A debug autopilot (builds sensibly at 3× speed) runs 20 in-game days without errors in the headless smoke test (§2.1).
- Screenshot review under Xvfb at the end of every milestone that changes visuals (§2.1).
- Profile 150 villagers + 40 enemies against the performance target in §5 (the C tick in the cloud; frame rate in the students' playtest).

**CI**

- Every PR: all jobs green (Linux GCC/Clang with sanitizers, Windows MSVC, C# build).

**Manual QA** — keep `docs/QA.md` as a checklist and walk it before each milestone ends: invalid placement; demolishing a building with workers; demolishing an occupied house at night; save → quit → continue; corrupted save offers the `.bak`; closing the window mid-raid; population 0; resources at cap; alt-tab; resolution and UI-scale changes; a Windows user folder with Korean characters.

---

## 9. Milestones

Each milestone ends **playable, tested, and committed**, with `PROGRESS.md` updated.

1. **Foundations:** repo, CMake (+ MinGW toolchain file), GitHub Actions CI, world create/destroy, terrain generation, tick, CLI skeleton, Godot project, bridge + ABI check, map rendering, camera, clock UI, headless smoke test.
2. **Colony loop:** placement, construction, demolish; villagers with the full walking handshake; production, consumption, storage, wages, mood, immigration and departure; roads; HUD and inspectors.
3. **World pressure:** seasons, nature events, ledger tooltips, market and merchant.
4. **Threats:** scheduling and warnings, raids and combat, walls, gate, towers, guards, damage and repair, loot, monsters and 마정석.
5. **Progression:** research tree, rank-ups and unlocks, win/lose, difficulty presets, tutorial objectives.
6. **Persistence and menus:** save slots, autosave, atomic saves with `.bak`, continue, settings, title and pause menus.
7. **Polish and balance:** art and audio pass, juice, overlays, simulator-driven balancing, performance, full QA checklist.

If time runs short, milestones 1–6 must be solid before 7. Cut content (fewer buildings, research nodes or events) before you cut stability.

---

## 10. Documentation

- `README.md` (Korean): what lives where; Windows prerequisites (Godot 4.7.2 .NET, .NET 8 SDK, CMake, Visual Studio Build Tools); how to pull a milestone branch, build the DLL and playtest; running the CLI and the game; the edit loop (stop the running game → rebuild C → press Play; the DLL is locked while the game runs).
- `docs/ARCHITECTURE.md` (Korean): diagram, ownership table, both handshakes, API list.
- `docs/C_REQUIREMENTS.md` (Korean): a table mapping each requirement in §1.1 to `file:function` and to the gameplay feature that uses it.
- `docs/C_WALKTHROUGH.md` (Korean): per C module — what it does, its key structs, the memory lifecycle of its data, and the tricky parts. End with 15 questions a teacher is likely to ask (e.g. "왜 포인터 배열을 썼나요?", "저장 파일에 포인터 대신 무엇을 저장하나요?") with short answers.
- `docs/DECISIONS.md`, `PROGRESS.md`, `docs/QA.md`, `docs/AI_USAGE.md`, `CREDITS.md`, and `docs/devlog/TEMPLATE.md` (날짜, 사람별 한 일, 문제와 해결, 다음 계획).
- `.gitignore` (build output, `game/.godot/`, `game/bin/`, `artifacts/`, IDE files).
- Keep the **Commands** section of `CLAUDE.md` current: the exact build, test, smoke-test and screenshot commands, so every new session can run them without rediscovering them.

---

## 11. Don't

- Don't put game rules or balance numbers in C#.
- Don't let Godot change game state except through the C API.
- Don't use the physics engine for units.
- Don't use any asset that isn't CC0 or OFL.
- Don't swallow errors silently; log them.
- Don't end a session with a broken build, failing tests, or unpushed commits.
- Don't push to `main` or merge PRs.
- Don't commit build output, downloaded tools, or screenshots (except the few chosen for `docs/screenshots/`).
