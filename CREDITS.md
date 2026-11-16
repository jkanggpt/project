# CREDITS

이 게임에 쓰인 외부 에셋과 라이선스입니다. 새 에셋을 추가하면 여기에도 적습니다.

## 그래픽

현재 화면은 `game/assets/painted/`의 새 그림을 사용합니다. 아래 Kenney 그림과 이전 합성 PNG는 원본 출처 기록과 보관용이며 현재 C 렌더러에서는 불러오지 않습니다.

| 현재 에셋 | 제작 방법 | 라이선스 | 원본·프롬프트 |
| --- | --- | --- | --- |
| painted/의 29 PNG: 주민 8종·적 6종 각 4프레임, 건물 15종·공사장, 수목·돌·광석·풀 12종, 지면·눈·도로, 자원 6종·화살 | 학생 요청과 첨부 화풍을 참고해 내장 image_gen으로 새로 생성, tools/pack_art.c로 자르기·알파 정리·크기 정렬 | 프로젝트 보유 권리를 CC0 1.0 헌정 (painted/LICENSE.txt) | art/generated/*.png, art/PROMPTS.md |

참고 첨부 이미지는 시각적 방향에만 사용했으며 게임·배포본에 복사하지 않았습니다. 원본 생성 파일도 같은 CC0 헌정 범위입니다. 생성 AI 사용은 docs/AI_USAGE.md에 별도로 표시합니다.

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
