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
