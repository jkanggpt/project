/*
 * format.h — CLI 용 한국어 문구
 *
 * C 코어는 언어를 모르고 숫자 코드만 냅니다. 사람이 읽을 문장은 프런트엔드가 만듭니다.
 * (Godot 쪽에는 Strings.cs 가 같은 일을 합니다)
 */
#ifndef CASTLE_CLI_FORMAT_H
#define CASTLE_CLI_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#include "castle_core.h"

const char *fmt_season_name(int32_t season);
const char *fmt_difficulty_name(int32_t difficulty);
const char *fmt_resource_name(int32_t resource);
const char *fmt_terrain_name(int32_t terrain);
/* 오류 코드를 사용자에게 보여 줄 문장으로 */
const char *fmt_result_message(int32_t code);
/* 이벤트 하나를 문장으로 */
void fmt_event(const CoreEventView *event, char *out, size_t size);
/* 조사 고르기: word 의 마지막 글자에 받침이 있으면 with_final("이", "을"), 없으면 without_final("가", "를") */
const char *fmt_josa(const char *word, const char *with_final, const char *without_final);
const char *fmt_building_name(int32_t type);
const char *fmt_place_result(int32_t result);
const char *fmt_villager_state(int32_t state);
const char *fmt_trait_name(const char *key);
const char *fmt_weather_name(int32_t weather);
const char *fmt_nature_name(int32_t event);
const char *fmt_deny_reason(int32_t reason);
const char *fmt_ledger_name(int32_t category);
const char *fmt_threat_name(int32_t type);
const char *fmt_rank_name(int32_t rank);
const char *fmt_branch_name(int32_t branch);
const char *fmt_research_state(int32_t state);
/* 연구 키 → 한국어 이름 (모르는 키는 그대로) */
const char *fmt_research_name(const char *key);
/* 연구 효과 설명 (예: "농장 생산 +25%") */
void fmt_effect(int32_t effect, int32_t value, char *out, size_t size);
const char *fmt_tutorial_name(int32_t step);
const char *fmt_lose_reason(int32_t reason);
const char *fmt_direction_name(int32_t direction);

/* "1년 봄 3일 (3일째) 06:00" */
void fmt_date(const CoreSummaryView *summary, char *out, size_t size);
/* 세이브 한 줄 요약 (성채 이름, 날짜, 등급, 주민, 난이도, 승패) */
void fmt_save_info(const CoreSaveInfoView *info, char *out, size_t size);

#endif
