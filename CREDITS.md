# CREDITS

## v1.2.4 그림 (2026-11-25)

내장 image_gen으로 의원·등대·고급 어선·두 도로 재질·신규5동물·풍차/관개농장 본체와 회전 부품·등대 조명·소형/고급 그물·항적·수확6단계를 제작했습니다. art/v124 원화와 V124_PROMPTS.json에 참조 및 생성 요청을 보존하고 tools/pack_v124_art.c로 패킹합니다. 사용자 후속 지시대로 전용 피격·사망은 적용/배포하지 않으며 동물 이동·수영·새끼는 모두4프레임만 사용합니다. 기존30종 걷기·공격·수영·새끼와 이전 알파카 원화를 재사용했습니다. 기존 산양·여우·뱀·타조의 누락된 공격 동작도 같은 외형을 참조해4프레임씩 새로 제작했습니다. 적용130목록 행과 추가 공용 아틀라스의 치수/SHA256은 art/V124_MANIFEST.json에 기록합니다. 프로젝트가 보유하는 새 원화와 파생 결과의 권리는 기존과 같이 CC0 1.0으로 헌정합니다.


## v1.2.3 그림 (2026-11-25)

`art/v123` 원화13장은 내장 image_gen으로 새로 제작했습니다. 채토장·급수탑·천문대·미술관의4방향, 청색·적색 화려한 천의 상인, 어선·연결 교량, 금/마정석과 토양 흔적, 주야 천문대 관측 애니메이션입니다. 기존 프로젝트 원화를 직접 참조했으며 어선 승객은 기존48주민을 C 렌더러에서 합성합니다. 패커 `tools/pack_v123_art.c`가 런타임 PNG23개를 만듭니다. 생성 요청/참조/보존 범위는 `art/V123_PROMPTS.json`, 파일별 치수/SHA256은 `art/V123_MANIFEST.json`입니다. 프로젝트가 보유하는 새 원화와 파생 결과의 권리는 CC0 1.0으로 헌정합니다. 사용자 첨부 문서/참고 그림 자체는 게임 이미지에 복사하지 않았습니다.

## v1.2.1 그림과 효과 (2026-11-20)

`art/v121` 원화15장은 내장 image_gen으로 생성했습니다. 새 주민2장·4재료 성문·6세력 습격6장·동물 공격·환상종·가축 새끼·계절 농장·온실 회전·도로 원화입니다. 전문 프롬프트는 `art/V121_PROMPTS.md`이며 프로젝트가 보유하는 권리는 CC0 1.0으로 헌정합니다. C 패커가 런타임12아틀라스를 만듭니다. 수목 원화는 앞선 v1.1.1 생성물을 다시 분리해 사용합니다. 비·천둥은 C 코드로 합성하며 외부 녹음은 사용하지 않습니다. 주민 이름은 학생 제공 ZIP의 목록을 사용했고 출처는 `game/data/NAMES_SOURCE.md`입니다.

이 게임에 쓰인 외부 에셋과 라이선스입니다. 새 에셋을 추가하면 여기에도 적습니다.

## 그래픽

현재 화면은 `game/assets/painted/`와 `game/assets/expansion/`의 새 그림을 사용합니다. 아래 Kenney 그림과 이전 합성 PNG는 원본 출처 기록과 보관용이며 현재 C 렌더러에서는 불러오지 않습니다.

| 현재 에셋 | 제작 방법 | 라이선스 | 원본·프롬프트 |
| --- | --- | --- | --- |
| expansion/animal_walk.png, wall_links.png: 동물16종×4걷기·성벽4재료×16연결 | 내장 image_gen 원화5장과 C 패커 | 프로젝트 보유 권리를 CC0 1.0 헌정 | art/expansion/animal_walk_0~3.png, wall_modules.png, art/TOPOLOGY_ANIMATION_PROMPTS.md |
| painted/의 29 PNG: 주민 8외형×3방향×4프레임·적 6종×4프레임, 건물 15종·공사장, 수목·돌·광석·풀 12종, 지면·눈·도로, 자원 6종·화살 | 학생 요청과 첨부 화풍을 참고해 내장 image_gen으로 새로 생성, tools/pack_art.c로 자르기·알파 정리·크기 정렬 | 프로젝트 보유 권리를 CC0 1.0 헌정 (painted/LICENSE.txt) | art/generated/*.png, art/PROMPTS.md, art/PROMPTS_OVERHEAD.md |

참고 첨부 이미지는 시각적 방향에만 사용했으며 게임·배포본에 복사하지 않았습니다. 원본 생성 파일도 같은 CC0 헌정 범위입니다. 생성 AI 사용은 docs/AI_USAGE.md에 별도로 표시합니다.

같은 날 후속 요청으로 현재 주민 그림을 갈색·금발·검은·붉은 머리 남녀 8종의 방향별 시트로 교체하고 건물도 약 45도 내려다보기로 다시 생성했습니다. 새 원본은 art/generated/villagers_{brown,blond,black,red}.png와 buildings_overhead.png, 프롬프트는 art/PROMPTS_OVERHEAD.md입니다. 직업별 주민·기존 건물 원본은 제작 이력으로만 보관합니다. 같은 CC0 헌정이 적용됩니다.
수목·돌·광맥·풀도 윗면이 보이는 새 원본 art/generated/props_overhead.png를 사용합니다.

| 에셋 | 제작 | 라이선스 | 위치 |
| --- | --- | --- | --- |
| [Tiny Town](https://kenney.nl/assets/tiny-town) | Kenney | CC0 | `game/assets/third_party/kenney/tiny-town/` |
| [Tiny Dungeon](https://kenney.nl/assets/tiny-dungeon) | Kenney | CC0 | `game/assets/third_party/kenney/tiny-dungeon/` |

## 효과음

| 에셋 | 제작 | 라이선스 | 위치 |
| --- | --- | --- | --- |
| [UI Audio](https://kenney.nl/assets/ui-audio) | Kenney | CC0 | `game/assets/third_party/kenney/audio-ui/` |
| [Interface Sounds](https://kenney.nl/assets/interface-sounds) | Kenney | CC0 | `game/assets/third_party/kenney/audio-interface/` |
| [RPG Audio](https://kenney.nl/assets/rpg-audio) | Kenney | CC0 | `game/assets/third_party/kenney/audio-rpg/` |
| [Impact Sounds](https://kenney.nl/assets/impact-sounds) | Kenney | CC0 | `game/assets/third_party/kenney/audio-impact/` |

마일스톤 4 부터 게임에서 씁니다 (`game/scripts/audio/Sfx.cs`): 습격 경보 종(`impactBell_heavy`), 화살(`drawKnife`), 근접 공격(`knifeSlice`), 성벽 타격(`impactWood_medium`), 무너짐(`impactPlank_medium`, `impactWood_heavy`), 적이 쓰러짐(`impactPunch_heavy`), 약탈(`handleCoins`), 습격이 끝남(`confirmation_002`).

## 글꼴

| 에셋 | 제작 | 라이선스 | 위치 |
| --- | --- | --- | --- |
| [Galmuri](https://github.com/quiple/galmuri) 2.40.3 (Galmuri11, Galmuri11 Bold) | Lee Minseo (quiple) | SIL Open Font License 1.1 (`game/assets/fonts/Galmuri-OFL.txt`) | `game/assets/fonts/` |

npm 패키지 `galmuri` 에서 받았습니다 (`npm pack galmuri`). 글꼴 파일은 고치지 않았습니다.

## 이 프로젝트에서 만든 그림

| 파일 | 만든 방법 | 라이선스 |
| --- | --- | --- |
| `game/assets/tiles/terrain.png` | `tools/make_art.py`: Kenney Tiny Town(CC0) 의 풀밭·나무 타일을 합성하고, 물·바위 지대·마정석 광맥 타일은 같은 팔레트로 절차적으로 그림 | CC0 (원본 Kenney CC0) |
| `game/assets/buildings/keep.png` | `tools/make_art.py`: Kenney Tiny Town(CC0) 성벽·문 타일 9개를 합성한 성채 그림 | CC0 (원본 Kenney CC0) |
| `game/assets/buildings/*.png` (주택, 농장, 채집 오두막, 벌목장, 채석장, 광산, 창고, 시장, 병영, 감시탑, 성벽, 성문, 치료소, 마탑), `construction_*.png` | `tools/make_art.py`: Kenney Tiny Town(CC0) 지붕·벽·문·울타리·밭 타일을 합성하고, 마탑·광산의 마정석은 같은 팔레트로 덧그림 | CC0 (원본 Kenney CC0) |
| `game/assets/units/villagers.png` | `tools/make_art.py`: Kenney Tiny Dungeon(CC0) 인물 8명을 한 줄로 모음 | CC0 (원본 Kenney CC0) |
| `game/assets/ui/res_*.png`, `job_*.png`, `bubble_*.png` | `tools/make_art.py`: Kenney Tiny Town·Tiny Dungeon(CC0) 아이템 타일과 직접 그린 8×8 말풍선 기호 | CC0 (원본 Kenney CC0) |
| `game/assets/tiles/roads.png` | `tools/make_art.py`: Kenney Tiny Town(CC0) 흙길 타일 2개 | CC0 (원본 Kenney CC0) |
| `game/assets/tiles/snow.png` | `tools/make_art.py`: 절차적으로 그린 눈 덮개 (외부 그림 없음) | CC0 |
| `game/assets/units/enemies.png` | `tools/make_art.py`: Kenney Tiny Dungeon(CC0) 의 적 6가지(도적, 두목, 고블린, 오우거, 병사, 궁수)를 한 줄로 모음 | CC0 (원본 Kenney CC0) |
| `game/assets/units/arrow.png`, `game/assets/ui/bubble_alarm.png` | `tools/make_art.py`: 직접 그린 8×8 화살과 경보 말풍선 (외부 그림 없음) | CC0 |

## 음악

아직 없습니다. CC0 곡을 찾으면 여기에 적습니다 (빈 자리는 마일스톤 7 에서 정리).

## C 그래픽 실행 라이브러리 (2026-11-14)

- raylib 5.5 — Ramon Santamaria 및 기여자, zlib/libpng 라이선스. https://github.com/raysan5/raylib/tree/5.5
- raylib가 포함하는 GLFW — zlib/libpng 라이선스. 배포 시 raylib와 GLFW 라이선스를 licenses/에 포함합니다.
- 현재 이미지는 2026-11-16 새 생성 그림으로 교체했습니다. 글꼴·효과음은 기존 CC0/OFL 파일을 사용합니다. 이전 Python 합성 그림의 제작 출처 기록은 보관합니다.
# 통합 확장 v1 그림 (2026-11-17)

`art/expansion/buildings_00.png`~`buildings_15.png`, `keep_turns.png`, `status_icons.png`, `resource_icons.png`, `animals.png`, `biomes.png`는 내장 image_gen으로 이 프로젝트를 위해 제작했습니다. 건물 65종의 정면·오른쪽·후면·왼쪽을 각각 생성했습니다. 밭에는 별도 주거 건물을 넣지 않았습니다.

`game/assets/expansion`의 방향별 건물 260장·아틀라스 4장·개별 자원 15장은 이 원본을 C 도구로 잘라 만든 파일입니다. 프로젝트가 보유하는 권리는 CC0 1.0으로 헌정합니다. 제작 프롬프트와 투명 배경 보정 기록은 `art/EXPANSION_PROMPTS.md`, `art/EXPANSION_ICONS_PROMPTS.md`, `art/expansion/ALPHA_REPAIR.md`에 있습니다. 참고용 첨부 이미지는 저장소·배포물에 포함하지 않았습니다.

## v1.1.1 그림 (2026-11-19)

`art/v111`의 `trees.png`, `manors.png`, `new_walk.png`, `swim.png`, `attack.png`, `biomes.png`, `roads.png`는 내장 image_gen으로 새로 제작했습니다.
수목20개·영주관6등급·동물 걷기/수영/공격·온수/냉수와 10생물군계·도로와 연결 타일을 `tools/pack_v111_art.c`로 패킹했습니다.
기존 동물 걷기 일부는 앞선 프로젝트 생성 원화를 유지했습니다. 프로젝트가 보유하는 권리는 CC0 1.0으로 헌정합니다.
전문 프롬프트·원본·투명 배경 보정은 `art/V111_PROMPTS.md`에 기록했습니다.

## v1.2.2 그림 (2026-11-22)

2026-11-23 후속 `art/v122_patch1/combat_fx.png`와 `factions.png`는 내장 image_gen으로 제작했습니다. 참격/화살/타격/발톱/불꽃/냉기/마법/폭발6프레임과6세력 문장입니다. 전문은 `art/V122_PATCH1_PROMPTS.json`이며 `tools/pack_ui_art.c`로 간격을 감지해 패킹합니다. 주민 프로필은 기존 평상복 원래 픽셀에서 어깨 위를 자른 파생 이미지입니다. 프로젝트가 보유하는 권리는 CC0 1.0으로 헌정합니다.

`art/v122`의 주민18쌍·업무복192장·농장류·영주관·채집장·군영·바위·생활 아이콘·광신도·코카트리스·히드라는 내장 image_gen으로 이 프로젝트를 위해 생성/편집했습니다. 기존 주민과 건물 원화를 직접 참고했고, 밝은 금발·앞뒤 활·투명 배경 교정까지 전문 프롬프트를 `art/V122_PROMPTS.md`와 연결된 JSON에 보관합니다. `tools/pack_v122_art.c`는 C로 자르기·발밑 정렬·아틀라스/메타데이터 패킹을 수행합니다. 기존12주민 프레임은 보존했습니다. 프로젝트가 보유하는 권리는 CC0 1.0으로 헌정합니다. 참고 첨부 이미지는 배포하지 않습니다.

