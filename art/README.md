# v1.2.3 원화와 패킹 — 2026-11-25

원화13장은 `art/v123`, 생성 요청/참조 기록은 [V123_PROMPTS.json](V123_PROMPTS.json), 치수·SHA256은 [V123_MANIFEST.json](V123_MANIFEST.json)입니다. 최초 시설4장의 누락된 프롬프트 전문은 요약으로 명시했고 보존된 요청은 전문으로 수록합니다.

`castle_v123_art`가 원화 셀을 분리해 `game/assets/expansion`에 PNG23개를 만듭니다. 새4시설은 방향별4장, 상인6×4(4걷기+2대기), 어선4×4, 교량4×4, 천문대4방향×주야2×관측8=64셀, 광석/토양/아이콘 시트를 포함합니다. 상인은 기존 주민 스타일에 청색·적색 천/금 장식, 어선은 기존48외형을 좌석에 합성합니다. 배/시설/천문대는 투명 경계와 방향별 공통 배율을 사용합니다.

```powershell
./build/msvc-check/bin/Release/castle_v123_art.exe
./build/msvc-check/bin/Release/castle_v123_art.exe --check game/assets/expansion
```

지도와 건설 카드는 최신 텍스처의 같은 소스 영역을 사용하며 원본 농장/성문 이미지로 되돌아가지 않습니다. 카드 색은 해금 등급이고 그림 색을 덧칠하지 않습니다. 모든 새 생성 원화 및 C 패킹 결과에 대해 프로젝트 보유 권리를 CC0 1.0으로 헌정합니다.

# v1.2.2 후속 그림 — 2026-11-23

공격 이펙트8종×6프레임과6세력 문장은 내장 image_gen으로 만들었습니다. 원본은 `v122_patch1`, 전문은 [V122_PATCH1_PROMPTS.json](V122_PATCH1_PROMPTS.json)입니다. `castle_ui_art`가 간격을 감지해 자르고 `game/assets/expansion/combat_fx.png`, `factions.png`로 패킹합니다. 같은 도구가 기본 주민의 실제 정면 프레임을 어깨 위까지 잘라 `portraits.png`48셀을 만듭니다. 프로필은 원래 외형을 보존합니다. 런타임에 농장의 투명 여백을 제외해 점유 칸에 맞춥니다. 자세한 적용은 `docs/V1_2_2_PATCH1.md`를 참고합니다.

# 새 중세 판타지 픽셀아트

## v1.2.2 원화 (2026-11-22)

추가 주민18쌍과 업무복192장 모두 기존4열×6행 원화를 직접 참조했습니다. 주민48종은 일상복576셀과 업무복4608셀을 사용합니다. 금발/전후 활과 투명 배경 교정 전문·원화 구성은 [V122_PROMPTS.md](V122_PROMPTS.md)에 있습니다. 농장5종은4방향×4계절, 과수원은 조성 전4방향을 추가했습니다. 영주관6단계·채집장·군영·바위·생활아이콘·광신도·코카트리스·히드라도 포함합니다.

최종 패킹은 `castle_v122_art`, 셀 검사는 `castle_v122_art --check game/assets/expansion`입니다. 일반복은12×48, 업무복4장은 각각24×48의96픽셀 셀입니다. 프로젝트가 보유하는 새 원화/패킹 결과의 권리도 기존과 같은 CC0 1.0으로 헌정합니다.

## v1.1.1 원화 (2026-11-19)

`art/v111`의 새 원본 7장은 수목 20개·영주관 6등급·추가 동물 9종·수영 5종·공격 9종·12지면·3도로입니다.
프롬프트 전문과 원본 대응은 [V111_PROMPTS.md](V111_PROMPTS.md)에 있습니다. 프로젝트가 보유하는 권리는 CC0 1.0으로 헌정합니다.
기존 확장 원화를 패킹한 다음 `castle_v111_art art/v111 game/assets/expansion`을 실행하고 `--check game/assets/expansion`으로 확인합니다.
투명한 행·열 사이의 골을 찾아 이웃 그림 혼입을 막고, 같은 동물의 4프레임은 동일 배율·발밑으로 정렬합니다.
실행 시에는 패킹된 `game/assets/expansion`만 필요합니다. 영주관은 정면 하나만 쓰며 회전하지 않습니다.

## 통합 확장판 원화 (2026-11-17)

현재 건물은 `art/expansion/buildings_00.png`~`buildings_15.png`, `keep_turns.png`의 65종×4방향입니다.
`status_icons.png`·`resource_icons.png`·`animals.png`·`biomes.png`도 새로 생성했습니다. 원본은 21장입니다.
프롬프트 전문은 `EXPANSION_PROMPTS.md`, `EXPANSION_ICONS_PROMPTS.md`, 투명도/셀 간섭 보정은 `expansion/ALPHA_REPAIR.md`입니다.

```powershell
./build/windows-msvc/bin/Release/castle_expansion_art.exe art/expansion game/assets/expansion
./build/windows-msvc/bin/Release/castle_expansion_art.exe --check game/assets/expansion
```

패커는 행별 투명한 골에서 4열을 잘라 전체 건물과 소품을 보존하고, 내용 경계로 자른 뒤 최근접 보간으로 맞춥니다.
출력은 `game/assets/expansion`의 260방향 PNG·4아틀라스·15개 자원 아이콘입니다. 런타임이 이 복사본을 읽으므로 생성 도구의 캐시 폴더는 배포에 필요하지 않습니다.
농장류는 밭·작물만 그립니다. 풍차 등 별도 건물은 따로 건설합니다. 지붕은 점유 칸보다 높게 그리며, 주민은 문에 도착해 실내로 들어갑니다.
`--screen rotations`로 4방향·점유 격자와 높이를 비교할 수 있습니다. 아래는 이전 그래픽 제작 이력입니다.

2026-11-16 학생 요청에 따라 캐릭터·건물·나무·광석·지면·도로·자원 아이콘·화살을 내장 image_gen 도구로 새로 제작했습니다. 이어진 요청에 따라 건물은 지붕이 더 잘 보이는 약 45도 내려다보기로 다시 그렸고, 주민은 직업과 무관한 외형 8종과 방향별 걷기로 교체했습니다. 첨부 그림은 화풍과 구도의 참고이며 자체 파일을 게임에 넣지 않았습니다.

| 원본 | 구성 | 게임 출력 |
| --- | --- | --- |
| generated/villagers_brown.png, villagers_blond.png, villagers_black.png, villagers_red.png | 머리색별 남녀 8종 × 정면/오른쪽/뒷면 × 걷기 4프레임 | painted/villagers.png (12열 × 8행, 셀 96×96), 왼쪽은 오른쪽 좌우 반전 |
| generated/enemies.png | 적 6종 × 걷기 4프레임 | painted/enemies.png (4열 × 6행, 셀 96×96) |
| generated/buildings_overhead.png | 지붕 윗면과 뒤쪽 깊이를 키운 건물 15종·공사장 | painted/성채 등 개별 PNG 16개 |
| generated/props_overhead.png | 윗면이 보이는 계절 수목 4종·돌 2종·광석 2종·풀/덤불/그루터기/갈대 | painted/props.png (4열 × 3행, 셀 256×256) |
| generated/ground.png | 지형 5종·눈, 각각 4변형 | painted/terrain.png (셀 64×64), snow.png |
| generated/icons.png | 자원 6종·화살·포장길 | painted/res_*.png, arrow.png, roads.png |

게임용 파일 경로의 공통 앞부분은 `game/assets/`입니다. 총 29개 PNG이며 글꼴·효과음은 기존 OFL/CC0를 유지합니다. 새 그림의 CC0 헌정은 `game/assets/painted/LICENSE.txt`에 기록했습니다. 현재 건물·주민의 프롬프트는 [PROMPTS_OVERHEAD.md](PROMPTS_OVERHEAD.md), 나머지는 [PROMPTS.md](PROMPTS.md)입니다. 이전 villagers.png와 buildings.png 원본은 제작 이력으로 보관하며 현재 패킹에는 사용하지 않습니다.

주민 외형 순서: 갈색 남성(청록 조끼)·갈색 여성(땋은 머리와 초록 앞치마), 금발 남성(파랑 튜닉)·금발 여성(포니테일과 황토색 조끼), 검은 머리 남성(짙은 청록 재킷)·검은 머리 여성(단발과 붉은 조끼), 붉은 머리 남성(초록 조끼)·붉은 머리 여성(반묶음과 보라 원피스). 직업 도구나 유니폼으로 구분하지 않습니다.

## C로 다시 패킹

현재 쓰는 원본 9장은 저장소에 포함되어 이미지 서비스나 Python 없이 같은 게임용 PNG를 다시 만들 수 있습니다. `CASTLE_BUILD_GAME=ON`, `CASTLE_BUILD_TOOLS=ON`으로 빌드합니다.

```powershell
./build/windows-msvc/bin/Release/castle_art.exe art/generated game/assets/painted
./build/windows-msvc/bin/Release/castle_art.exe --check game/assets/painted
```

다시 패킹한 뒤 게임을 다시 빌드하면 실행 파일 옆 assets 폴더도 갱신됩니다. 전체 새 그림을 다시 생성하는 작업에는 이미지 생성 도구가 필요합니다.

`pack_art.c`는 생성 이미지의 실제 크기에 따라 셀을 나눕니다. 현재 소품은 4열×3행, 건물은 4열×4행, 머리색별 주민 원본은 4열×6행입니다. 성벽은 가운데를 잘라 한 칸 벽체로 만듭니다. 알파 96 미만의 옅은 외곽을 없앤 뒤 투명 영역을 자르고 최근접 보간으로 축소합니다. 캐릭터와 소품은 아래쪽 중앙에 정렬합니다. 그림만 바꿔도 CMake 빌드의 castle_game_assets가 실행 폴더로 동기화합니다.

## 점유와 높이

- 판정·경로·저장에서는 기존 16×16 논리 타일과 건물 정의의 w×h를 사용합니다.
- 주민 발밑은 실제 이동 좌표이고 그림 높이는 24논리 픽셀, 즉 1.5타일입니다. 머리카락과 팔은 점유 칸 밖으로 나올 수 있습니다.
- 주민 ID로 외형을 정하므로 직업 변경·승급·저장/로드 때 같은 외형을 유지합니다. 이동 방향을 기억해 정면/오른쪽/뒷면/왼쪽으로 전환하고, 실제 이동 거리로만 4프레임을 진행합니다. 정지하면 마지막 방향의 중립 자세, 일시정지하면 걷기가 멈춥니다.
- 건물 그림은 앞쪽 지면 `(y+h)×16`에 고정합니다. 원본 비율을 유지하면서 성채는 점유 높이보다 최소 24픽셀, 마탑은 최소 36픽셀 높게 그립니다. 지붕과 처마는 점유 영역 바깥에도 보입니다.
- 나무·돌·광맥은 지면과 분리한 소품입니다. 건물 점유 칸과 도로 위에는 소품을 그리지 않습니다. 앞뒤 칸의 수관은 겹칠 수 있습니다.
- 모든 건물·수목·주민·적을 발밑 y로 함께 정렬합니다. 뒤 주민은 지붕에 가리고 앞 주민은 건물 위에 보입니다. 지붕의 꼭대기 y는 정렬에 사용하지 않습니다.
- 건설 미리보기는 같은 높이의 반투명 그림과 실제 점유 칸 테두리를 함께 표시합니다. 건물 선택 테두리도 실제 점유 칸입니다.

핵심 구현은 `game/src/sprite_layout.c`, `session.c`, `render.c`, `assets.c`입니다. `native_runtime`은 높이·기준점·앞뒤 순서·외형·실제 이동 중 방향과 정지를, `native_art_assets`는 PNG 크기·내용·투명도와 주민 96프레임 각각의 발밑·내용·걷기 변화 여부를 검사합니다.

`castle_game --screenshot artifacts/villagers.png --screen villagers --no-audio`로 게임과 같은 선택 함수를 쓰는 8외형×4방향 비교 화면을 촬영할 수 있습니다.

## 2026-11-17 성벽 연결과 동물 걷기

내장 image_gen으로 `art/expansion/animal_walk_0~3.png`, `wall_modules.png`를 새로 제작했습니다. 전체 프롬프트는 [TOPOLOGY_ANIMATION_PROMPTS.md](TOPOLOGY_ANIMATION_PROMPTS.md)에 있습니다. C 패커는 원화를 잘라 `game/assets/expansion/animal_walk.png`(16종×4프레임)와 `wall_links.png`(4재료×16연결)를 만듭니다. 새 그림도 CC0입니다.

동물의 코어 좌표·판정은 유지하고 화면 위치를 보간합니다. 실제 움직인 거리로 걷기 프레임을 바꾸고, 왼쪽 이동은 같은 옆모습을 반전합니다. 정지·일시정지 중에는 걷기를 진행하지 않습니다.
성벽은 회전과 관계없이 상·우·하·좌의 정상 벽/문을 검사해 직선·모서리·T·교차 그림을 고릅니다. 모든 방향은 같은 중심 기둥과 반쪽 연결 부재를 사용합니다.
확장 건물의 그림 높이·발밑은 유지하고 너비를 점유 너비의1.1배+4논리 픽셀 이내로 제한합니다. 밭은 그대로 점유 칸에 맞춥니다.
`castle_expansion_art --check game/assets/expansion`은64동물 프레임의 내용·발밑·변화,64벽 연결의 가장자리와 투명도를 검사합니다. `--screen connections`로 실제 게임의 그림 선택을 확인할 수 있습니다.


## v1.2.1 그림

`art/v121`의15개 새 원본과 `V121_PROMPTS.md`를 사용합니다. `castle_v121_art art/v121 game/assets/expansion`으로 패킹하고 `castle_v121_art --check game/assets/expansion`으로 검사합니다. 실행 게임에 Python이나 이미지 서비스가 필요하지 않습니다.

런타임은 `villagers_v121.png`(12외형×3방향×4프레임), `raider_walk.png`/`raider_attack.png`(28종×3방향×4프레임), `animal_walk_v121.png`/`animal_attack_v121.png`/`animal_babies.png`/`animals_v121.png`, `trees_v121.png`, `gates_v121.png`, `farms_v121.png`, `greenhouse_turns.png`, `road_connections.png`입니다. 생성 원화의 행·열 간격은 균등하다고 가정하지 않고 투명 경계를 전역 탐색합니다. 나무는 연결 성분별로 분리합니다. 마지막 공격 복귀 프레임에는 온전한 준비 자세를 사용합니다.

성문은 목책문/석조문/성문/철갑문을 해당 성벽 재료와 같은 화면 크기로 표시합니다. 밀밭·관개농장·과수원·유리 온실은 계절별 그림을 사용합니다. 도로는8이웃 연결 마스크로, 교량 난간은 실제 인접 물에 맞춰 C에서 선택합니다. 새 그림에 대해 프로젝트가 보유하는 권리는 CC0입니다.

## v1.2.4

새 원화는 v124/, 프롬프트는 V124_PROMPTS.json, 실제 적용 파일/치수/해시는 V124_MANIFEST.json입니다. castle_v124_art art/v124 game/assets/expansion 으로 패킹하고 --check 출력폴더 docs/v124_art_source.csv 로 검사합니다. 사용자 후속 지시대로 전용 피격/사망70행을 제외하고 동물 이동·수영·새끼는4프레임입니다. 신규 원화의 걷기/수영0·2·4·6장과 새끼 앞4장만 패킹하며 다른 상태는 적용하지 않습니다. 기존30종4걷기와 공격/수영/새끼를 그대로 재사용합니다. 풍차/수차의 본체/부품/카드 좌표와 선박 좌석은 game/src/sprite_layout.c를 공유합니다. 그물/조명8장과 선체/항적4장은 동물 프레임 변경과 별개입니다.
