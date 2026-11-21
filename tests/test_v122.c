/* test_v122.c — 변경 규칙의 경계와 저장 이관 회귀 검사 */
#include "expansion.h"
#include "test_util.h"
#include <stdio.h>
#include <string.h>
void test_v122_rules_and_save(void) {
    CoreNewGameParams p={0};World *w=NULL,*loaded=NULL;int32_t line=0;
    strcpy(p.castle_name,u8"v122 저장 시험");p.seed=7;p.difficulty=1;p.ruleset=1;
    CHECK_EQ(core_create(&p,NULL,&w),CORE_OK);if(!w)return;
    CHECK_EQ(CORE_BLD_COUNT,66);CHECK_EQ(CORE_SAVE_VERSION,10);
    CHECK_EQ(core_expansion_tree_mature_age(3),48000);
    CHECK_EQ(expansion_spec(CORE_BLD_COTTAGE)->w,2);CHECK_EQ(expansion_spec(CORE_BLD_COTTAGE)->h,3);
    CHECK_EQ(expansion_spec(CORE_BLD_BARRACKS)->rank,1);CHECK_EQ(expansion_spec(CORE_BLD_GARRISON)->workers,6);
    for(int i=0;i<w->expansion->cells;i++)CHECK(w->expansion->tiles[i].stone_milli>0);
    CHECK(w->expansion->water_count>=5);int large=0;
    for(int i=0;i<w->expansion->water_count;i++)large+=w->expansion->water_area[i]>=250;
    CHECK(large>0);CHECK_EQ(core_validate(w),0);
    CHECK_EQ(core_tick(w,50),CORE_OK);CHECK_EQ(core_save(w,"v122_roundtrip.sav"),CORE_OK);
    int rc=core_load("v122_roundtrip.sav",NULL,&loaded,&line);
    if(rc!=CORE_OK)fprintf(stderr,"v122 load result=%d line=%d\n",rc,line);
    CHECK_EQ(rc,CORE_OK);
    if(loaded){uint64_t a,b;core_state_hash(w,&a);core_state_hash(loaded,&b);CHECK_EQ(a,b);core_destroy(loaded);}
    core_destroy(w);
    if(rc==CORE_OK){remove("v122_roundtrip.sav");remove("v122_roundtrip.sav.bak");}
}
