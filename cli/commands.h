/*
 * commands.h — 대화형 메뉴
 *
 * 메뉴는 "명령 표"(이름 + 실행 함수 포인터)로 만들어집니다.
 * 표에 한 줄을 추가하면 메뉴 항목이 생기고, 고른 번호의 함수가 불립니다.
 */
#ifndef CASTLE_CLI_COMMANDS_H
#define CASTLE_CLI_COMMANDS_H

#include "castle_core.h"

#define CLI_PATH_BYTES 260

typedef struct CliContext {
    CoreWorld *world; /* 지금 진행 중인 게임 (없으면 NULL) */
    int running;      /* 0 이 되면 메뉴를 끝냄 */
    char save_path[CLI_PATH_BYTES];
} CliContext;

/* 메뉴 명령 하나. run 은 성공하면 1, 입력이 끝났으면(EOF) 0 */
typedef int (*CliCommandFn)(CliContext *context);

typedef struct CliCommand {
    const char *label;
    CliCommandFn run;
    int needs_world; /* 게임이 있어야 쓸 수 있는 명령이면 1 */
} CliCommand;

/* 메인 메뉴를 돌립니다. 게임이 없으면 새 게임/불러오기부터 묻습니다 */
void cli_run_menu(CliContext *context);
/* 파일에서 게임을 불러와 context 의 게임으로 삼습니다. 성공 1 */
int cli_load_game(CliContext *context, const char *path);

#endif
