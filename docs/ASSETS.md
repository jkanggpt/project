# 아트·오디오·폰트

런타임 정본은 game/assets/assets_manifest.json입니다. 논리ID·실제 경로·종류·치수·격자·프레임·SHA256/CRC·권리·생산자·해제 책임을 한 번 정의합니다. CMake가 이 원장으로 C 인덱스를 만들며 로더·검사기·복사·설치가 같은 목록을 사용합니다. 문자열 조합으로 불러오는 건물4방향, 영주관6단계, 자원15종도 모두 원장에 등록합니다.

실제 게임 그림·음원·폰트407개는 정리 전과 동일한 바이트입니다. 권리 고지5개를 포함한 현재 원장 총412개를 재생성 및 대조합니다. 동물 전용 피격·사망 그림은 만들지 않습니다. 걷기·수영·새끼는4프레임이며 기존 공격 모양과 방향별 효과를 유지합니다.

art/art_manifest.json은 실제 생성 입력329개와 발행 리소스를 연결합니다. art/recipes/pipeline.json이 단계 순서, art/recipes/provenance.json이 기존 요청의 전문을 보존합니다. 프롬프트 근거 안의 과거 표기/참조 문자열은 근거 자료 예외이며 실행·빌드·로딩에 사용하지 않습니다. 파일 경로로 명령을 실행하지 않습니다.

단일 진입점 castle_art_pipeline은 base → buildings → nature → scenery → residents → effects/manor → services → production 순서입니다. 각 단계는 일반 C 모듈이고 공통 image_ops/trace만 공유합니다. 입력을 덮어쓰지 않고 별도 work에 중간 시트를 만든 뒤 원장에 있는 출력만 발행합니다. 모든 실제 읽기/쓰기는 ARTINPUT/ARTOUTPUT으로 추적할 수 있습니다.

재생성 명령과 별도 출력 검사 방법은 BUILD_AND_TEST.md를 참고하세요. 그림 로딩은 크기와 필수ID를 검사하고 누락을 명확히 기록합니다. 픽셀 필터·알파·프레임 자르기는 정리 전 결과와 동일하게 유지합니다. 권리 고지는 CREDITS.md와 game/assets/licenses(설치본 assets/licenses)를 따릅니다.
