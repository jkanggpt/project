# 제작·라이선스

게임은 C11과 raylib 5.5로 제작했습니다. 프로그램 코드의 별도 공개 라이선스는 저장소에 지정되어 있지 않습니다. 사용한 외부 라이브러리 고지는 설치본 licenses 폴더에 포함합니다.

| 자료 | 제작자·권리 | 현재 경로 |
|---|---|---|
| 생성 그림과 파생 스프라이트 | 프로젝트가 보유하는 권리를 CC0 1.0으로 헌정 | game/assets/licenses/generated-art.txt |
| UI·전투 효과음 8개 | Kenney, CC0 | game/assets/audio, game/assets/licenses/kenney-audio-*.txt |
| Galmuri11 글꼴 | Lee Minseo (quiple), SIL OFL 1.1 | game/assets/fonts/Galmuri11.ttf, game/assets/licenses/Galmuri-OFL.txt |
| raylib 5.5와 포함된 GLFW | Ramon Santamaria 및 기여자, zlib/libpng 계열 | third_party/raylib-5.5.tar.gz, 설치본 licenses |
| 주민 이름 324개 | 학생 제공 이름 목록 | game/data/names.txt, docs/NAME_SOURCES.md |

원화는 내장 image_gen으로 제작했으며, 사용자 참고 이미지는 화풍·시점의 참고로만 사용했습니다. 참고 이미지 자체를 배포 이미지에 복사하지 않았습니다. 원본 입력은 art/sources, 생성 요청 전문과 당시 메타데이터는 art/recipes/provenance.json, 입력·출력 SHA256과 권리 연결은 art/art_manifest.json에 있습니다. 현재 그림 생성 명령은 castle_art_pipeline 하나입니다.

현재 동물의 이동·수영·새끼 동작은 4프레임입니다. 사용자 지시에 따라 동물 전용 피격·사망 그림은 제작·적용하지 않습니다. 생성형 AI 사용 범위는 docs/AI_USAGE.md를 참고하세요. 음악은 포함하지 않습니다.

조명 실험실의 9종 무조명 재질 원화와 재질 노멀은 내장 image_gen으로 생성했습니다. 기하 기준 표면으로 만든 실험용 스프라이트·월드 노멀·높이/발광 데이터와 함께 프로젝트가 보유하는 권리를 CC0 1.0으로 헌정합니다. 원본은 art/sources/lighting_lab, 요청 전문은 art/recipes/lighting_prototype.json과 provenance.json에 보존합니다. 실험 셰이더·도구 코드는 기존 프로그램 코드와 같은 정책을 따릅니다.

기존 픽셀 원화 조명 비교는 게임의 오두막·참나무·주민·잔디·돌길·바위 그림을 그대로 추출해 사용합니다. 원본 비교 시트와 image_gen으로 제작한 명암 완화·세부 법선 지침은 art/sources/lighting_pixels에 있고, 생성 요청과 적용 범위는 art/recipes/pixel_lighting.json 및 provenance.json에 기록했습니다. 원본과 해당 파생 그림에 대해 프로젝트가 보유하는 권리는 기존 생성 그림과 같은 CC0 1.0 정책을 따릅니다.

## GPU 풍경의 픽셀 마스크

`art/sources/gpu_scenery/masks.txt`의 풀·연기 마스크와 팔레트 및 재생성 코드는 Codex가 이 프로젝트를 위해 작성했습니다. 외부 그림이나 예제 코드를 가져오지 않았으며, 기존 게임 원화의 색감에 맞춘 여덟 변형을 `tools/art/gpu_scenery.c`로 재생성합니다. 생성 결과와 원본의 권리는 프로젝트의 기존 생성 아트 정책을 따릅니다.

자연 풍경의 연속 재료·초지·물결·연기와 알파 접촉 거리장 코드는 Codex가 작성했습니다. `art/sources/gpu_scenery/contacts.txt`의 기술 규약은 같은 아트 파이프라인으로 재생성하며 요청과 제작 범위는 `art/recipes/provenance.json`에 기록합니다. 접촉 거리장은 기존 게임 원화의 실제 알파에서 GPU로 만드는 기술 자료입니다. 참고 그림의 픽셀이나 외부 셰이더 코드는 복제하지 않았으며, 원본 그림의 권리는 그대로 유지합니다.

세계 스프라이트 354개 시트의 알베도·법선 쌍은 기존 생성 원화를 `tools/art/sprite_materials.c`로 자동 처리한 기술 자료입니다. 프레임 목록·표면 규약은 `art/sources/sprite_materials`, 전체 제작 근거는 `art/recipes/sprite_materials.json`과 `provenance.json`에 있습니다. 새 외부 그림이나 생성 요청은 추가하지 않았습니다. 자동 추정 법선과 제한된 원광 명암 완화이며 수작업 법선 원화로 표시하지 않습니다. 이 파생 자료에 대한 프로젝트 보유 권리는 원본과 같은 CC0 1.0 정책을 따릅니다.
