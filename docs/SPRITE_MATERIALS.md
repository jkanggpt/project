# 세계 스프라이트의 알베도·법선

기존 2D 원화의 354개 시트, 7,304개 프레임 칸에 알베도·세계 공간 법선을 추가했습니다. 방향별 건물·부착물·농장·성장 단계·계절 나무·바위·주민 복장과 동작·적·동물·배·그물이 포함됩니다. 빈 종·동작 칸도 원래 격자로 보존하므로 칸 수는 실제로 그려진 개체 수와 다릅니다.

| 분류 | 시트 수 | 그리기 경로 |
|---|---:|---|
| 고정 원화 재료 | 354 | 처음 사용할 때 알베도·법선을 읽고 GPU에서 조명 계산 |
| 자연 지면·해안·도로 | 7 | 기존 연속 절차 재료·물결 법선 |
| 등대 빛·배의 물결 효과 | 12 | 자체 발광·효과 합성 경로 |
| 전체 세계 텍스처 | 373 | 목록 누락을 자동 검사 |

## 제작 방식과 한계

`art/sources/sprite_materials/catalog.txt`는 실제 64·96·128·256화소 프레임과 예외 크기를 기록합니다. `profiles.txt`는 지붕·벽·수관·바위·인물·동물·배·지면의 표면 규약입니다. 원화 제작 단계가 끝난 뒤 `tools/art/sprite_materials.c`가 셀을 독립 처리합니다. 정수 거리장과 재료별 큰 표면 방향을 만들고 비슷한 색 안의 밝기 차이만 제한된 미세 요철로 더합니다. 주황색 수관과 나무 기둥, 색 지붕과 벽을 구분합니다.

추정한 원광의 방향 명암을 제한된 공통 RGB 배율로 완화합니다. 짙은 선·유리색·원본 알파와 투명 여백은 보존합니다. 완전한 무조명 색이나 가려진 실제 입체 형상을 한 장의 그림에서 복원한 것은 아닙니다. 354개 시트는 **자동 생성·연결 완료**이며 모든 셀의 수작업 법선 제작·시각 검수 완료를 뜻하지 않습니다. 기존 여섯 보정 표본은 해당 원화 조각에서 계속 우선 사용합니다.

알베도는 원화 해상도, 법선은 셀 경계를 나누어떨어지게 하는 1·2·4분의 1 해상도입니다. 법선의 RGB는 세계 X·Y·Z 방향이며, 반전 시 X를 뒤집습니다. 누운 스프라이트는 표면 방향을 지면으로 돌린 뒤 그림의 회전을 적용합니다. 표본이 프레임 밖을 읽지 않도록 각 자르기 영역을 제한합니다.

## 실행과 자원 수명

세계 그림만 재료 쌍으로 교체합니다. UI·인벤토리·건설 미리보기는 원본 `GameAssets`를 사용합니다. 재료 쌍은 게임 창 수명 동안 재사용하고 `game_gpu_shutdown`에서 모두 해제합니다. 매 프레임 파일 읽기나 굽기를 하지 않습니다. 처음 나타난 시트의 PNG 해독·업로드 비용은 해당 첫 프레임에 발생합니다. 메뉴로 돌아가도 같은 재료를 다시 만들지 않으며, 메뉴 왕복 검사에 재료 쌍 개수와 바이트 수도 포함했습니다.

전체 재료를 모두 보았을 때의 추가 해독 자료는 알베도 약 394.65 MiB, RGB 법선 약 59.88 MiB입니다. 실제로 본 시트만 읽으므로 보통 이보다 작습니다. 바이트 집계는 화소 형식 기준이며 드라이버 내부 할당 비용은 별도입니다. 조명 개수·광원 배치·큰 그림자 체적은 이번 변경 범위 밖입니다.

## 제작·검사 명령

기본 빌드와 실행에는 Python이 필요 없습니다. 다음 도구는 GPU 없이도 실행합니다. Windows에서는 같은 빌드의 `.exe` 경로를 사용합니다.

```sh
cmake --preset linux-release -DCASTLE_WERROR=ON
cmake --build --preset linux-release
build/linux-release/bin/castle_sprite_materials --check art/sources game/assets
build/linux-release/bin/castle_sprite_materials --gallery game/assets build/reports/sprite-materials
ctest --test-dir build/linux-release -R 'sprite_material|assets_' --output-on-failure
```

비교 시트는 왼쪽부터 원화·알베도·법선·네 방향 조명입니다. `gallery.tsv`에 각 행의 원본과 자르기 좌표가 남습니다. 비교 조명은 CPU의 단순 방향 조명이며 최종 게임 색 보정·그림자 비교와 구별합니다. 대표 48개 자르기 영역을 네 장으로 검수하며 전체 칸의 육안 검수로 확대 집계하지 않습니다.

입력이나 규약을 수정할 때는 다음 순서로 원장을 갱신합니다. `publish_sprite_materials.py`는 C 도구가 만든 PNG의 크기·해시·메타데이터만 읽습니다.

```sh
build/linux-release/bin/castle_sprite_materials --bake art/sources game/assets game/assets
python3 tools/publish_sprite_materials.py
cmake --preset linux-release -DCASTLE_WERROR=ON
cmake --build --preset linux-release
ctest --test-dir build/linux-release -R 'assets_regenerate|assets_reproducible' --output-on-failure
```

## 검증 기록

검사 결과와 화면 캡처는 `build/reports/sprite-materials`에 보관합니다. 알파·선화 보존, 밝기 보정 한도, 법선 길이·위쪽 방향, 프레임 독립, 세계 목록 누락, 원장 연결, SHA256 및 재생성 일치를 검사합니다. `gpu_sprite_materials`는 실제 OpenGL에서 원화 법선 샘플·좌우 반전·회전한 지면 방향과 지면 표식을 확인합니다. 게임의 `GAME MATERIALS` 진단에는 읽은 재료 쌍과 자동 추정 경로 사용 횟수가 출력됩니다.

| 검사 | 결과 |
|---|---|
| Linux Release 전체 WERROR 빌드·CTest | 289/289 통과 |
| 최종 굽기 뒤 영향 검사 | 재생성·재현성·원장·재료 계약·고정 표본·GPU 방향 6/6 통과 |
| 원본·입력·출력 SHA256 | 기존 원본 변경 없음, 1,128개 출력·338개 입력 일치 |
| 대표 시각 검수 | 37개 시트의 48개 자르기 영역, 네 조명 방향 |
| 실제 게임 화면 | 제목·튜토리얼·게임 × 800×600·1024×768·1440×900·1920×1080 = 12장 |
| ASan·UBSan 메뉴 왕복 | 5주기·120프레임·40회 경로 전환, 오류 0·종료 후 공용 GPU 자원 0 |
| 재료 캐시 왕복 | 모든 주기 11쌍·59,887,616바이트로 고정, 미등록 추정 경로 0 |
| 실제 게임 표본 | 14쌍·61,735,036바이트, 미등록 추정 경로 0 |

메뉴 검사에는 제목 → 새 게임·튜토리얼·이어하기·불러오기 → 제목 경로가 포함됩니다. LSan은 기존에 확인한 Mesa `libGLX_mesa.so.0` 할당에만 필터를 적용했습니다. 제외된 드라이버 할당은 481개·108,124바이트이며 게임 코드의 메모리 오류·누수 보고는 없었습니다. 이 검사는 Linux 소프트웨어 OpenGL의 기능·수명 검사이며 RTX 성능 측정으로 표시하지 않습니다.

Windows MSVC는 PR #6의 해당 커밋 CI에서 `cmake --preset windows-msvc -DCASTLE_WERROR=ON`, 전체 빌드와 CTest로 검증합니다. hosted Windows에는 GPU가 없으므로 그 작업은 GPU 실행 검사를 포함하지 않습니다. 실제 OpenGL 검사는 위 Linux 결과와 구분합니다.
