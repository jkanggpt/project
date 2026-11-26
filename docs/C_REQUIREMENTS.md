# C 요구사항 대응표

| 요구사항 | 현재 구현 | 실제 역할 |
|---|---|---|
| 구조체와 포함·참조 | core/src/entity.h, world.h, expansion.h | 주민·건물·세계·동물·어선의 소유 관계 |
| 포인터와 원본 변경 | core/src/common/entities.c, economy/bank.c | 배치·자원·목록 갱신 |
| 사용자 정의 함수 | core/include/castle_core.h, castle_expansion.h | 공개 API와 역할별 내부 함수 |
| 함수 포인터 | common/definitions.c, persistence/save.c, cli/commands.c | 정의/저장 파서, 메뉴 디스패치 |
| 동적 메모리 | common/entities.c, world/grid.c, common/events.c | 엔티티 목록·격자·이벤트 큐의 할당과 해제 |
| 배열·문자열 | buildings/catalog.c, ecology/species.c | 현재 건물·동물 카탈로그, UTF-8 이름 |
| 파일 입출력 | persistence/save.c, persistence/content_save.c, common/definitions.c | 검증된 저장과 정의 파일 읽기·쓰기 |
| 오류 처리 | persistence/save.c, common/platform.c, cli/console.c | 손상 입력 거부·원자 교체·잘못된 메뉴 입력 재질문 |
| 모듈 분리 | core/CMakeLists.txt, game/CMakeLists.txt | 공용 코어·공용 실행기·화면·CLI의 정상 링크 |

코어 공개 헤더의 선언은 한 벌입니다. UI에는 복사 가능한 뷰를 반환하고 내부 배열 포인터의 소유권을 넘기지 않습니다. 생성 실패 시 중간 할당을 해제하고 저장 실패 시 기존 세계를 교체하지 않습니다. 대응 동작은 CTest의 메모리 실패·손상 저장·메뉴 입력·공용 실행기 사례에서 확인합니다.
