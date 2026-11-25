/* v122_ui.c — 코어와 같은 수치를 보여 주는 주민·시설·전투 검사창 */
#include "app.h"
#include "format.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static float clamp01(float n){return fminf(1,fmaxf(0,n));}
static Rectangle area(float x,float y,float w,float h){return (Rectangle){x,y,w,h};}
const char *game_faction_name(int faction) {
    static const char *const names[]={u8"도적",u8"마물",u8"언데드",u8"숲",u8"북방",u8"광신도"};
    return names[faction>=0&&faction<6?faction:0];
}
void game_portrait(GameApp *a,int appearance,Rectangle box) {
    if(appearance<0||appearance>=48)appearance=0;
    DrawRectangleRec(box,UI_BG);
    DrawTexturePro(a->assets.portraits,(Rectangle){appearance%8*64.0f,appearance/8*64.0f,64,64},box,(Vector2){0,0},0,WHITE);
    DrawRectangleLinesEx(box,1,UI_EDGE);
}
void game_stat_numbers(GameApp *a,const int *values,float x,float y,float gap) {
    static const char *const names[]={u8"힘",u8"민첩",u8"체력",u8"손재주",u8"지능"};
    for(int i=0;i<5;i++) {
        ui_small(&a->ui,x+i*gap,y,game_stat_color(values[i]),"%d",values[i]);
        if(CheckCollisionPointRec(GetMousePosition(),area(x+i*gap-2,y-2,gap,26)))
            snprintf(a->ui.tooltip,sizeof(a->ui.tooltip),u8"%s %d · 왼쪽부터 힘 / 민첩 / 체력 / 손재주 / 지능",names[i],values[i]);
    }
}
Color game_stat_color(int value) {
    static const unsigned colors[10]={0xF5F5F5FF,0xFFF59DFF,0xFDE047FF,0xA3E635FF,0x22C55EFF,0x38BDF8FF,0x3B82F6FF,0x9B5DE5FF,0xF032B0FF,0xEF4444FF};
    return GetColor(colors[value<1?0:value>10?9:value-1]);
}
void game_select(GameApp *a,int kind,int id) {
    a->selection_kind=id?kind:0;a->selection_id=id;
    a->selected_building=kind==1?id:0;a->selected_villager=kind==2?id:0;a->inspector_scroll=0;
}
void game_building_label(const GameSession *s,int id,char *out,size_t capacity) {
    const CoreBuildingView *b=NULL;
    for(int i=0;i<s->building_count;i++)if(s->buildings[i].id==id){b=&s->buildings[i];break;}
    if(!b){snprintf(out,capacity,u8"없음");return;}
    if(b->type==CORE_BLD_KEEP)snprintf(out,capacity,u8"영주관");
    else snprintf(out,capacity,"%s#%d",core_expansion_building_name(b->type),b->type_serial);
}
static void life_icons(GameApp *a,float x,float y,int icon,int count,float proportion) {
    Texture2D t=a->assets.life_icons;int quarters=(int)roundf(clamp01(proportion)*count*4);
    for(int i=0;i<count;i++) {
        Rectangle dest={x+i*27,y,23,23},source={icon*64.0f,0,64,64};
        if(t.id) {
            DrawTexturePro(t,source,dest,(Vector2){0,0},0,(Color){80,80,85,255});
            int q=quarters-i*4;if(q<0)q=0;if(q>4)q=4;
            if(q){source.width*=q/4.0f;dest.width*=q/4.0f;DrawTexturePro(t,source,dest,(Vector2){0,0},0,WHITE);}
        } else {
            DrawRectangleLinesEx(dest,1,UI_MUTED);DrawRectangleRec(area(dest.x,dest.y,dest.width*clamp01((quarters-i*4)/4.0f),dest.height),UI_GOLD);
        }
    }
}
static const char *const fields[12]={u8"농업",u8"임업",u8"광업",u8"축산",u8"어업",u8"수렵",u8"가공",u8"경비",u8"의료",u8"교역",u8"학술",u8"마법"};
static const char *const skill_names[4]={u8"초보",u8"도제",u8"직인",u8"장인"};
static Color skill_color(int level){return GetColor((unsigned[]){0x9CA3AFFF,0x6366F1FF,0xE11D48FF,0xFACC15FF}[level]);}
static int skill_level(int minutes){return minutes>=core_expansion_skill_threshold(3)?3:minutes>=core_expansion_skill_threshold(2)?2:minutes>=core_expansion_skill_threshold(1)?1:0;}
float game_person_details(GameApp *a,const CoreVillagerView *v,float x,float y,float width) {
    GameUi *u=&a->ui;char label[180],job[120];game_building_label(&a->session,v->workplace_id,job,sizeof(job));
    game_portrait(a,v->appearance,area(x,y,64,64));
    ui_small(u,x+74,y+2,UI_GOLD,u8"%s #%d",v->name,v->personal_id);
    ui_small(u,x+74,y+31,UI_MUTED,"%s",fmt_villager_state(v->state));y+=74;
    ui_small(u,x,y,UI_TEXT,u8"일터: %s",job);y+=27;
    CoreWorkSchedule schedule;core_expansion_schedule(a->session.world,v->id,&schedule);
    ui_small(u,x,y,UI_MUTED,u8"근무 %02d:%02d~%02d:%02d %s",schedule.start/60,schedule.start%60,schedule.end/60,schedule.end%60,schedule.on_duty?u8"근무조":u8"비번");y+=26;
    if(schedule.second_end){ui_small(u,x,y,UI_MUTED,u8"후반 %02d:%02d~%02d:%02d",schedule.second_start/60,schedule.second_start%60,schedule.second_end/60,schedule.second_end%60);y+=25;}
    if(v->workplace_id && a->session.building_defs[v->job].category==CORE_CAT_TOWERS) {
        for(int shift=0;shift<2;shift++)if(ui_button(u,area(x+shift*width/2,y,width/2-3,28),shift?u8"야간조":u8"주간조",1,schedule.shift==shift,u8"동료와 교대 · 하루 1회 변경")) {
            int rc=core_expansion_set_shift(a->session.world,v->id,shift);
            if(rc!=CORE_OK)snprintf(a->message,sizeof(a->message),u8"오늘 이미 변경했거나 전투 중입니다.");
            (void)game_session_refresh(&a->session);
        }
        y+=35;
    }
    if(v->workplace_id && ui_button(u,area(x,y,width,30),u8"일터 선택·범위 보기",1,0,job))game_select(a,1,v->workplace_id);
    if(v->workplace_id)y+=38;
    const int values[5]={v->strength,v->agility,v->vitality,v->dexterity,v->intellect};
    static const char *const names[5]={u8"힘",u8"민첩",u8"체력",u8"손재주",u8"지능"};
    static const int thresholds[9]={12,24,40,66,90,124,172,236,300};
    for(int i=0;i<5;i++) {
        int value=values[i],maximum=value>=10||value>=v->initial_stats[i]+3;
        Color color=game_stat_color(value);
        ui_small(u,x,y,color,u8"%s %d/10",names[i],value);
        if(maximum)ui_small(u,x+150,y,UI_MUTED,"MAX");
        else ui_small(u,x+116,y,UI_MUTED,"%.1f/%d XP",v->stat_xp[i]/1000.0,thresholds[value-1]);
        y+=24;
        for(int n=0;n<10;n++)DrawRectangleRec(area(x+n*width/10,y,width/10-2,7),n<value?color:UI_BG);
        y+=10;ui_bar(area(x,y,width,3),maximum?1:v->stat_xp[i]/(thresholds[value-1]*1000.0f),maximum?UI_MUTED:color);y+=14;
    }
    int skill=a->skill_tab;if(skill<0||skill>=12)skill=0;
    for(int i=0;i<12;i++) {
        int level=skill_level(v->skill_minutes[i]);
        if(ui_button(u,area(x+i%3*width/3,y+i/3*30,width/3-3,27),fields[i],1,skill==i,skill_names[level]))a->skill_tab=i;
        DrawRectangleLinesEx(area(x+i%3*width/3,y+i/3*30,width/3-3,27),1,skill_color(level));
    }
    y+=127;int level=skill_level(v->skill_minutes[skill]);float days=v->skill_minutes[skill]/540.0f;
    ui_small(u,x,y,skill_color(level),u8"%s · %s · %.1f/120일",fields[skill],skill_names[level],days);y+=26;
    ui_bar(area(x,y,width,9),days/120.0f,skill_color(level));y+=17;
    if(level==3)ui_small(u,x,y,skill_color(level),u8"MAX · 장인");
    else ui_small(u,x,y,UI_MUTED,u8"다음 단계까지 %.1f일",core_expansion_skill_threshold(level+1)/540.0-days);
    y+=30;
    int32_t efficiency=100;core_expansion_profession_efficiency(a->session.world,v->id,skill,&efficiency);
    ui_small(u,x,y,UI_GOLD,u8"%s 능률 E %.2f",fields[skill],efficiency/100.0);y+=28;
    if(skill==7) {int32_t ranged=100;core_expansion_efficiency(a->session.world,v->id,CORE_BLD_TOWER,&ranged);ui_small(u,x,y,UI_MUTED,u8"방어탑 능률 E %.2f",ranged/100.0);y+=26;}
    const char *const life_names[]={u8"행복도",u8"기분",u8"포만",u8"휴식",u8"안전"};
    float proportions[]={v->happiness/100.0f,(v->mood+200)/400.0f,v->hunger/100.0f,v->rest/100.0f,v->safety/100.0f};
    for(int i=0;i<5;i++) {
        int value=(int[]){v->happiness,v->mood,v->hunger,v->rest,v->safety}[i];
        snprintf(label,sizeof(label),i==1?u8"%s %+d/200":u8"%s %d/100",life_names[i],value);
        ui_small(u,x,y,UI_TEXT,"%s",label);life_icons(a,x+122,y, i,i==0?5:4,proportions[i]);y+=31;
    }
    ui_small(u,x,y,UI_TEXT,"HP %d/%d",v->health*v->combat_max_hp/100,v->combat_max_hp);y+=25;
    ui_bar(area(x,y,width,12),v->health/100.0f,UI_GOOD);y+=25;
    static const char *const causes[29] = {
        u8"식사",        u8"물 마시기", u8"갈증 발생",   u8"식수 부족",
        u8"배고픔 시작", u8"허기 지속", u8"충분한 수면", u8"근무 성취",
        u8"장시간 근무", u8"안전 산책", u8"교육 방문",   u8"보호구역 안전",
        u8"일상 피로",   u8"위험 목격", u8"근무조 적응", u8"습격 불안",
        u8"근접 위협",   u8"피격",      u8"중상",        u8"사망 목격",
        u8"습격 종료",   u8"문화 방문", u8"식수 중단",   u8"통근 피로",
        u8"습격 목격",   u8"대피",      u8"수면 방해",   u8"공동체 사망 충격",
        u8"미술관 관람"};
    if (v->mood_cause >= 32 && v->mood_cause < 61)
        ui_small(u, x, y, UI_MUTED, u8"%s %+d · %d일 %02d:%02d", causes[v->mood_cause - 32],
                 v->mood_event_amount, v->mood_event_tick / 144 + 1, (v->mood_event_tick % 144) * 10 / 60,
                 (v->mood_event_tick % 6) * 10);
    else ui_small(u,x,y,UI_MUTED,u8"기분 변화: 생활·문화·부상 사건");
    y+=30;
    if(v->mood_event_source) {
        const char *source=NULL;CoreRaiderInfo enemy;CoreAnimalInfo animal;
        if(v->mood_source_kind==1 && v->mood_source_type>0 && core_expansion_raider_info(v->mood_source_type-1,&enemy)==CORE_OK)source=enemy.name;
        if(v->mood_source_kind==2 && v->mood_source_type>0 && core_expansion_animal_info(v->mood_source_type-1,&animal)==CORE_OK)source=animal.name;
        if(v->mood_source_kind==3){game_building_label(&a->session,v->mood_event_source,label,sizeof(label));source=label;}
        if(source){ui_small(u,x,y,UI_MUTED,u8"원인: %s #%d",source,v->mood_event_source);y+=27;}
    }
    return y;
}
float game_facility_details(GameApp *a,const CoreBuildingView *b,float x,float y,float width) {
    CoreFacilityView f;GameUi *u=&a->ui;
    if(core_expansion_facility(a->session.world,b->id,b->type,b->x,b->y,b->rotation,&f)!=CORE_OK)return y;
    if(b->id)ui_small(u,x,y,UI_TEXT,u8"배치 %d/%d · 현장 %d · 일일 환산 %.2f",f.assigned,b->worker_slots,f.present,f.labor_milli/1000.0);
    else ui_small(u,x,y,UI_MUTED,u8"건설 후보 · 기준 F=1 · 근무 전");
    y+=28;
    int32_t weights[5];
    if(core_expansion_weights(b->type,weights)==CORE_OK) {
        ui_small(u,x,y,UI_MUTED,u8"힘%d%% · 민첩%d%% · 체력%d%%",weights[0],weights[1],weights[2]);y+=24;
        ui_small(u,x,y,UI_MUTED,u8"손재주%d%% · 지능%d%%",weights[3],weights[4]);y+=26;
        int sum=0;
        for(int i=0;i<a->session.villager_count;i++) {
            const CoreVillagerView *worker=&a->session.villagers[i];if(!b->id || worker->workplace_id!=b->id)continue;
            int32_t efficiency=0;core_expansion_efficiency(a->session.world,worker->id,b->type,&efficiency);sum+=efficiency;
        }
        ui_small(u,x,y,UI_GOLD,u8"총 능률 F %.2f (E의 합)",sum/100.0);y+=29;
    }
    if(b->state==CORE_BSTATE_CONSTRUCTING) {
        ui_wrap(u,area(x,y,width,55),UI_GOLD,core_expansion_build_locked(a->session.world)?u8"습격 중 공사 중단 · 적 철수 완료 후 재개":u8"공사 중 · 생산 시작 전");y+=58;
    }
    if(f.radius) {ui_small(u,x,y,UI_MUTED,u8"작업 반경 %d · 명목 %.1f/일",f.radius,f.nominal_milli/1000.0);y+=27;}
    if(f.range){ui_small(u,x,y,SKYBLUE,u8"공격 %d · 사거리 %d · 주기 %d분",f.attack,f.range,f.cooldown);y+=27;}
    if(b->job_field==1) {ui_wrap(u,area(x,y,width,66),UI_TEXT,TextFormat(u8"성목 %d그루\n일반 원목 %.1f · 마력수 원목 %.1f",f.mature_trees,f.logs_milli/1000.0,f.magic_logs_milli/1000.0));y+=70;}
    if(b->job_field==2)for(int i=0;i<CORE_RES_COUNT;i++) {
        int resource=core_expansion_display_resource(i);if(!f.deposits[resource]||(b->type==CORE_BLD_CLAY_PIT&&resource!=CORE_RES_CLAY))continue;
        ui_small(u,x,y,UI_TEXT,u8"%s 재고 %.1f",fmt_resource_name(resource),f.deposits[resource]/1000.0);y+=25;
    }
    if(f.annual_yield){ui_small(u,x,y,UI_TEXT,u8"연간 기준 수확 %d · 실제 작기 노동 반영",f.annual_yield);y+=28;}
    for(int i=0;i<CORE_RES_COUNT;i++) {
        int r=core_expansion_display_resource(i);
        if(f.nominal_input[r]){ui_small(u,x,y,UI_MUTED,u8"소비 %s · 기준 %.1f / 계산 %.1f",fmt_resource_name(r),f.nominal_input[r]/1000.0,f.actual_input[r]/1000.0);y+=25;}
        if(f.nominal_output[r]){ui_small(u,x,y,UI_TEXT,u8"생산 %s · 기준 %.1f / 계산 %.1f",fmt_resource_name(r),f.nominal_output[r]/1000.0,f.actual_output[r]/1000.0);y+=25;}
    }
    if(f.fuel_milli){ui_small(u,x,y,UI_MUTED,u8"계산 연료 %.2f/일",f.fuel_milli/1000.0);y+=25;}
    if(f.stopped_reason){ui_small(u,x,y,UI_BAD,"%s",fmt_deny_reason(f.stopped_reason));y+=27;}
    if(f.trade_base){ui_small(u,x,y,UI_TEXT,u8"거래 %d/%d · 기준 %d",f.trade_used,f.trade_limit,f.trade_base);y+=25;ui_small(u,x,y,UI_MUTED,u8"직원 E %.2f · 다음 재입고 %d일",f.trade_efficiency/100.0,f.trade_next_day);y+=27;}
    if(!b->id)return y;
    if(b->worker_slots&&!f.present){ui_small(u,x,y,UI_BAD,u8"현장 직원 없음 · 근무·이동 상태 확인");y+=28;}
    int tower=a->session.building_defs[b->type].category==CORE_CAT_TOWERS;
    for(int group=0;group<(tower?2:1);group++) {
    int members=0;
    if(tower){ui_small(u,x,y,UI_GOLD,"%s",group?u8"야간 · 18:00~06:00":u8"주간 · 06:00~18:00");y+=30;}
    for(int i=0;i<a->session.villager_count;i++) {
        const CoreVillagerView *v=&a->session.villagers[i];if(v->workplace_id!=b->id)continue;
        CoreWorkSchedule schedule;core_expansion_schedule(a->session.world,v->id,&schedule);
        if(tower && schedule.shift!=group)continue;
        members++;
        char label[180];snprintf(label,sizeof(label),u8"%s · %s%s",v->name,v->mission==CORE_MISSION_SCOUT?u8"정찰":v->mission==CORE_MISSION_PATROL?u8"순찰":fmt_villager_state(v->state),v->id==b->combat_guard_id?u8" · 전투 담당":"");
        if(ui_button(u,area(x,y,width,92),"",1,0,label))game_select(a,2,v->id);
        game_portrait(a,v->appearance,area(x+3,y+3,42,42));
        ui_small(u,x+52,y+3,UI_TEXT,"%s",v->name);
        ui_small(u,x+52,y+25,UI_MUTED,"%s%s",v->id==b->combat_guard_id?u8"전투 담당 · ":"",fmt_villager_state(v->state));
        int values[5]={v->strength,v->agility,v->vitality,v->dexterity,v->intellect};
        game_stat_numbers(a,values,x+7,y+59,36);int32_t efficiency=0;core_expansion_efficiency(a->session.world,v->id,b->type,&efficiency);
        ui_small(u,x+width-96,y+59,UI_GOLD,"E %.2f",efficiency/100.0);y+=99;
    }
    if(tower && !members){ui_small(u,x,y,UI_BAD,u8"미배치");y+=29;}
    if(tower && !members && ui_button(u,area(x,y,width,30),group?u8"야간 일꾼 선택":u8"주간 일꾼 선택",1,0,NULL)) {
        a->assign_shift=group;game_app_open(a,PANEL_ASSIGN);
    }
    if(tower && !members)y+=39;
    }
    return y;
}
int game_unit_details(GameApp *a,float x,float *yp,float width) {
    if(a->selection_kind!=3&&a->selection_kind!=4)return 0;
    GameSession *s=&a->session;float y=*yp;char text[700];int hp=0,maximum=1,px=0,py=0;
    if(a->selection_kind==3) {
        CoreAnimalView *animal=NULL;CoreAnimalInfo info;
        for(int i=0;i<s->animal_count;i++)if(s->animals[i].id==a->selection_id){animal=&s->animals[i];break;}
        if(!animal){ui_text(&a->ui,x,y,UI_MUTED,u8"처치됨 / 사라짐");*yp=y+40;return 1;}
        px=animal->x_milli/1000;py=animal->y_milli/1000;core_expansion_animal_info(animal->kind,&info);
        if(s->visibility[py*s->summary.map_width+px]<2){ui_text(&a->ui,x,y,UI_MUTED,u8"현재 시야 밖");*yp=y+40;return 1;}
        hp=animal->hp;maximum=info.hp;
        snprintf(text,sizeof(text),u8"%s #%d · %s\n성향 %s · 생물량 %d\n공격 %d · 주기 %d분 · 사거리 %d\n속도 %.2f · 수영 %.2f\n행동: %s\n전리품 식량%d / 가죽%d / 마정석%d\n지역 %d · 나이 %d일",
          info.name,animal->id,animal->owner?u8"가축":animal->kind>=24?u8"환상종":u8"야생",info.behavior==2?u8"공격적":info.behavior==1?u8"중립":u8"온순",info.biomass,info.attack,info.attack_minutes,info.attack_range,info.speed_milli/1000.0,info.swim_milli/1000.0,animal->attack_remaining_milli?u8"공격":animal->retaliation_until>s->summary.tick?u8"반격":u8"이동",info.food,info.leather,info.mana_drop,animal->region,animal->age);
        if(animal->owner) {
            char home[120];game_building_label(s,animal->owner,home,sizeof(home));
            int mature=animal->kind==7?24:animal->kind==8?16:animal->kind==9?12:8;
            size_t n=strlen(text);snprintf(text+n,sizeof(text)-n,u8"\n소속 %s · 사료: 식량\n굶은 날 %d · %s · 성숙까지 %d일",home,animal->starve_days,animal->age>=mature?u8"성체":u8"유년",animal->age>=mature?0:mature-animal->age);
        }
    } else {
        GameEnemy *enemy=game_enemy(s,a->selection_id);
        if(!enemy||enemy->hp<=0){ui_text(&a->ui,x,y,UI_MUTED,u8"처치됨 / 사라짐");*yp=y+40;return 1;}
        px=(int)enemy->actor.position.x;py=(int)enemy->actor.position.y;
        if(s->visibility[py*s->summary.map_width+px]<2){ui_text(&a->ui,x,y,UI_MUTED,u8"현재 시야 밖");*yp=y+40;return 1;}
        hp=enemy->hp;maximum=enemy->def.hp;
        static const char *const factions[]={u8"도적",u8"마물",u8"언데드",u8"숲",u8"북방",u8"광신도"};
        static const char *const abilities[]={u8"없음",u8"아군 공격 +10%",u8"성벽 피해 2배",u8"성벽 통과",u8"해골 소환 · 2시간",u8"아군 치유 · 2시간",u8"성벽 피해 +50%",u8"냉기 둔화 25%",u8"범위 충격",u8"사망 폭발",u8"범위 심판"};
        snprintf(text,sizeof(text),u8"%s #%d · %s\n공격 %d · 주기 %d분 · 사거리 %d\n이동속도 %.2f · %s\n현재: %s\n소환자 #%d\n공격기간 남음 %.1f시간",
            enemy->info.name,enemy->actor.id,factions[enemy->info.faction],enemy->def.damage,enemy->def.cooldown,enemy->def.range,enemy->def.speed/1000.0,abilities[enemy->info.ability>=0&&enemy->info.ability<11?enemy->info.ability:0],enemy->fleeing?u8"철수":enemy->attack_serial&&enemy->attack_elapsed<4.8f?u8"공격":u8"접근",enemy->summoner_id,fmaxf(0,(s->raid.assault_end_tick-s->summary.tick)/6.0f));
    }
    ui_wrap(&a->ui,area(x,y,width,350),UI_TEXT,text);y+=355;
    ui_small(&a->ui,x,y,UI_GOOD,"HP %d/%d",hp,maximum);y+=28;ui_bar(area(x,y,width,14),hp/(float)maximum,UI_GOOD);y+=32;
    *yp=y;return 1;
}
void game_raid_alert(GameApp *a) {
    if(!a->raid_alert_id)return;
    a->ui.input_blocked=0;a->ui.tooltip[0]='\0';
    GameSession *s=&a->session;float width=fminf(960,GetScreenWidth()-40.0f),height=fminf(760,GetScreenHeight()-40.0f);
    Rectangle box={(GetScreenWidth()-width)/2,(GetScreenHeight()-height)/2,width,height};
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){0,0,0,185});ui_box(box,UI_PANEL);
    int faction=s->raid.type-1;if(faction<0||faction>5)faction=0;
    DrawTexturePro(a->assets.factions,area(faction%3*128.0f,faction/3*128.0f,128,128),area(box.x+24,box.y+16,84,84),(Vector2){0,0},0,WHITE);
    DrawTextEx(a->assets.font,game_faction_name(faction),(Vector2){box.x+124,box.y+21},34,0,UI_GOLD);
    ui_small(&a->ui,box.x+124,box.y+66,UI_BAD,u8"습격 발견 · 최초 발견/공격부터 48시간 · 일시정지");
    int found=0;for(int n=0;n<s->enemy_count;n++) {
        GameEnemy *e=&s->enemies[n];int p=(int)e->actor.position.y*s->summary.map_width+(int)e->actor.position.x;
        found+=e->hp>0&&p>=0&&p<s->cells&&s->visibility[p]>=2;
    }
    ui_small(&a->ui,box.x+28,box.y+114,UI_TEXT,u8"발견 %d명 · 최초 편성 %d명 · 생존 %d명 · 공격기간 남음 %.1f시간",found,s->raid.initial_count,s->raid.alive,fmaxf(0,(s->raid.assault_end_tick-s->summary.tick)/6.0f));
    int kinds=0;for(int n=0;n<28;n++)kinds+=s->raid.initial_types[n]>0;
    int columns=width>=860?2:1,rows=(kinds+columns-1)/columns;
    Rectangle list=area(box.x+24,box.y+153,width-48,height-234);
    int maximum=(int)fmaxf(0,rows*148.0f-list.height);
    if(CheckCollisionPointRec(GetMousePosition(),list))a->raid_scroll-=(int)(GetMouseWheelMove()*70);
    if(a->raid_scroll<0)a->raid_scroll=0;
    if(a->raid_scroll>maximum)a->raid_scroll=maximum;
    ui_clip_begin((int)list.x,(int)list.y,(int)list.width,(int)list.height);
    int slot=0;
    for(int type=0;type<28;type++)if(s->raid.initial_types[type]) {
        float cw=(list.width-10)/columns;
        Rectangle card=area(list.x+(slot%columns)*cw,list.y+(slot/columns)*148-a->raid_scroll,cw-8,139);slot++;
        if(card.y+card.height<list.y||card.y>list.y+list.height)continue;
        ui_box(card,UI_BG);CoreEnemyDefView def;CoreRaiderInfo info;
        core_get_enemy_def(s->world,type,&def);core_expansion_raider_info(type,&info);
        Rectangle src={(type%4*4+1)*96.0f,(type/4*3)*96.0f,96,96};
        DrawTexturePro(a->assets.enemies,src,area(card.x+5,card.y+17,96,108),(Vector2){0,0},0,WHITE);
        float tx=card.x+107;ui_small(&a->ui,tx,card.y+10,UI_GOLD,u8"%s ×%d",info.name,s->raid.initial_types[type]);
        ui_small(&a->ui,tx,card.y+40,UI_TEXT,u8"HP %d · 공격 %d",def.hp,def.damage);
        ui_small(&a->ui,tx,card.y+67,UI_TEXT,u8"사거리 %d · 주기 %d분",def.range,def.cooldown);
        ui_small(&a->ui,tx,card.y+94,UI_MUTED,u8"속도 %.2f · %s",def.speed/1000.0,def.range>1?u8"원거리":u8"근접");
    }
    ui_clip_end();
    if(maximum>0)ui_bar(area(box.x+width-19,list.y,4,list.height),a->raid_scroll/(float)maximum,UI_GOLD);
    if(ui_button(&a->ui,area(box.x+24,box.y+height-61,width-48,38),u8"확인하고 계속",1,0,u8"경보가 만든 정지만 해제합니다.")) {
        a->raid_alert_id=0;a->speed=a->raid_alert_speed;a->raid_scroll=0;
    }
}
