# 새 중세 판타지 픽셀아트

2026-11-16 학생 요청에 따라 캐릭터·건물·나무·광석·지면·도로·자원 아이콘·화살을 내장 image_gen 도구로 새로 제작했습니다. 첨부 주민 시트와 파란 지붕 병영은 화풍·재질·높이의 참고입니다. 첨부 파일 자체는 게임에 넣지 않았습니다.

| 원본 | 구성 | 게임 출력 |
| --- | --- | --- |
| generated/villagers.png | 직업 8종 × 걷기 4프레임 | painted/villagers.png (4열 × 8행, 셀 96×96) |
| generated/enemies.png | 적 6종 × 걷기 4프레임 | painted/enemies.png (4열 × 6행, 셀 96×96) |
| generated/buildings.png | 건물 15종·공사장 | painted/성채 등 개별 PNG 16개 |
| generated/props.png | 계절 수목 4종·돌 2종·광석 2종·풀/덤불/그루터기/갈대 | painted/props.png (4열 × 3행, 셀 256×256) |
| generated/ground.png | 지형 5종·눈, 각각 4변형 | painted/terrain.png (셀 64×64), snow.png |
| generated/icons.png | 자원 6종·화살·포장길 | painted/res_*.png, arrow.png, roads.png |

게임용 파일 경로의 공통 앞부분은 `game/assets/`입니다. 총 29개 PNG이며 글꼴·효과음은 기존 OFL/CC0를 유지합니다. 새 그림의 CC0 헌정은 `game/assets/painted/LICENSE.txt`에 기록했습니다. 생성 프롬프트는 [PROMPTS.md](PROMPTS.md)입니다.

## C로 다시 패킹

그림 원본은 저장소에 포함되어 이미지 서비스나 Python 없이 같은 게임용 PNG를 다시 만들 수 있습니다. `CASTLE_BUILD_GAME=ON`, `CASTLE_BUILD_TOOLS=ON`으로 빌드합니다.

```powershell
./build/windows-msvc/bin/Release/castle_art.exe art/generated game/assets/painted
./build/windows-msvc/bin/Release/castle_art.exe --check game/assets/painted
```

다시 패킹한 뒤 게임을 다시 빌드하면 실행 파일 옆 assets 폴더도 갱신됩니다. 전체 새 그림을 다시 생성하는 작업에는 이미지 생성 도구가 필요합니다.

`pack_art.c`는 생성 이미지의 실제 크기에 따라 셀을 나눕니다. props 원본의 실제 여백에 맞춰 0–400 / 400–700 / 700–1024 비율로 행을 자르고, 성벽은 가운데를 잘라 한 칸 벽체로 만듭니다. 알파 96 미만의 옅은 외곽을 없앤 뒤 투명 영역을 자르고 최근접 보간으로 축소합니다. 캐릭터와 소품은 아래쪽 중앙에 정렬합니다.

## 점유와 높이

- 판정·경로·저장에서는 기존 16×16 논리 타일과 건물 정의의 w×h를 사용합니다.
- 주민 발밑은 실제 이동 좌표이고 그림 높이는 24논리 픽셀, 즉 1.5타일입니다. 무기와 팔은 점유 칸 밖으로 나올 수 있습니다.
- 건물 그림은 앞쪽 지면 `(y+h)×16`에 고정합니다. 원본 비율을 유지하면서 성채는 점유 높이보다 최소 24픽셀, 마탑은 최소 36픽셀 높게 그립니다. 지붕과 처마는 점유 영역 바깥에도 보입니다.
- 나무·돌·광맥은 지면과 분리한 소품입니다. 건물 점유 칸과 도로 위에는 소품을 그리지 않습니다. 앞뒤 칸의 수관은 겹칠 수 있습니다.
- 모든 건물·수목·주민·적을 발밑 y로 함께 정렬합니다. 뒤 주민은 지붕에 가리고 앞 주민은 건물 위에 보입니다. 지붕의 꼭대기 y는 정렬에 사용하지 않습니다.
- 건설 미리보기는 같은 높이의 반투명 그림과 실제 점유 칸 테두리를 함께 표시합니다. 건물 선택 테두리도 실제 점유 칸입니다.

핵심 구현은 `game/src/sprite_layout.c`, `render.c`, `assets.c`입니다. `native_runtime`은 높이·기준점·앞뒤 순서·직업 매핑을, `native_art_assets`는 PNG 크기·내용·투명도를 검사합니다.
