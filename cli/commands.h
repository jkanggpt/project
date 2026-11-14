/*
 * commands.h — 대화형 메뉴
 *
 * 메뉴는 "명령 표"(이름 + 실행 함수 포인터)로 만들어집니다.
 * 표에 한 줄을 추가하면 메뉴 항목이 생기고, 고른 번호의 함수가 불립니다.
 */
#ifndef CASTLE_CLI_COMMANDS_H
#define CASTLE_CLI_COMMANDS_H

#include "castle_core.h"
#include "battle.h"
#include "walker.h"

#define CLI_PATH_BYTES 260

typedef struct CliContext {
    CoreWorld *world;       /* 지금 진행 중인 게임 (없으면 NULL) */
    const CoreDefs *defs;   /* 정의 (data 폴더에서 읽은 것) */
    Walker walker;          /* 주민 걷기 흉내 */
    Battle battle;          /* 습격을 숫자로 풀기 (C 그래픽 프런트엔드 의 전투 대신) */
    int running;            /* 0 이 되면 메뉴를 끝냄 */
    char save_path[CLI_PATH_BYTES];
} CliContext;

/* 메뉴 명령 하나. run 은 성공하면 1, 입력이 끝났으면(EOF) 0 */
typedef int (*CliCommandFn)(CliContext *context);

typedef struct CliCommand {
    const char *label;
    CliCommandFn run;
    int needs_world; /* 게임이 있어야 쓸 수 있는 명령이면 1 */
} CliCommand;

/* 메인 메뉴를 돌립니다 */
void cli_run_menu(CliContext *context);
/* 파일에서 게임을 불러와 context 의 게임으로 삼습니다. 성공 1 */
int cli_load_game(CliContext *context, const char *path);
/* 쌓인 이벤트를 모두 출력합니다 (생산 이벤트는 개수만 모아서) */
void cli_print_events(CoreWorld *world);
/* 한 칸 글자 (지도 보기용) */
char cli_building_symbol(int32_t type);

#endif
