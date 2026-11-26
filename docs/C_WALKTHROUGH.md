# 현재 C 코드 읽는 순서

1. core/include의 두 헤더에서 외부에 제공하는 명령·뷰·ID를 확인합니다. core/src/world.h와 expansion.h는 내부 세계와 현재 상태의 소유권을 정의합니다.
2. core/src/common/api.c와 world/state.c에서 생성·틱·조회·해제를 따라갑니다. 모든 정상 월드는 160×160의 현재 모델입니다.
3. world/generator.c, resource_budget.c에서 지도와 자원 예산을 봅니다. 예산 투영은 별도 게임 모드를 만들지 않고 동일 시드 결과를 보존합니다.
4. buildings/catalog.c, commands.c, placement.c에서 건설·점유·도로·회전·보호구역을 확인합니다.
5. people/routines.c, wellbeing.c, workforce.c에서 실제 출근·생활·능률과 경험을 따라갑니다. economy/production.c와 harvest.c는 공유 자원과 수확을 처리합니다.
6. ecology와 services에서 동물·가축·의료·급수·연구·사건 예보·어선을 봅니다. combat/raids.c는 현재 습격 일정을 담당합니다.
7. game/src/runtime의 GameSession이 실제 이동·전투를 실행합니다. 콘솔도 같은 세션으로 시간을 진행합니다. game/src/render와 ui는 이 상태를 표시하고 명령을 요청합니다.
8. core/src/persistence는 필드 표·CRC·임시 세계를 사용해 현재 저장 하나만 읽습니다. 저장 후 미래 틱까지 같은 결과인지 검사합니다.
9. tools/art_pipeline.c와 tools/art는 입력 원화에서 현재 출력 전체를 생성합니다. 경로·치수·프레임과 SHA는 매니페스트로 검사합니다.
10. tests의 기능별 파일과 CLEANUP_AUDIT.md에서 실제 검증 범위와 남은 한계를 확인합니다.

주민·건물의 점유와 이미지 높이는 다릅니다. game/src/render/sprite_layout.c에서 발밑과 그림 상자를 계산하며, 화면 크기를 바꾸어도 코어 점유·통행 규칙은 바뀌지 않습니다. 다음 변경도 기존 역할 모듈과 기능별 테스트를 직접 갱신합니다.
