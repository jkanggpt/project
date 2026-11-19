/* main.c — Godot와 .NET 없이 실행하는 C 게임의 진입점과 한 판의 흐름 */
#include "app.h"
#include "format.h"
#include "sim.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

typedef struct Options {
    const char *data,*assets,*save_dir,*screenshot,*screen;
    int seed, difficulty, days, smoke, stress, probe, frames, no_audio, width, height, autoplay, wildlife;
} Options;
static int number(const char *text,int min,int max,int *out)
{
    char *end; long n; errno=0; n=strtol(text,&end,10);
    if (!*text || *end || errno || n<min || n>max) return 0;
    *out=(int)n; return 1;
}
static void help(void)
{
    puts("Magic Castle - C / raylib\ncastle_game [--data dir] [--assets dir] [--save-dir dir]\n"
         "  --smoke-test [--days 20] [--seed 42]  C runtime test without a window\n"
         "  --stress | --stress-probe              150 villagers + 40 enemies\n"
         "  --screenshot file.png [--screen title|game|villagers|population|rotations|connections|research|rank|market|load]\n"
         "  --frames N --autoplay --no-audio --width 1440 --height 900\n"
         "  --difficulty peaceful|normal|harsh (or 0|1|2)");
}
static int options(int argc,char **argv,Options *o)
{
    int i;
    memset(o, 0, sizeof(*o));
    o->seed = 42;
    o->difficulty = 1;
    o->days = 20;
    o->width = 1440;
    o->height = 900;
    o->screen = "title";
    o->wildlife = 180;
    for (i=1;i<argc;i++) {
        const char *arg=argv[i];
        if (!strcmp(arg,"--smoke-test") || !strcmp(arg,"--headless")) { o->smoke=1; continue; }
        if (!strcmp(arg,"--stress")) { o->stress=1; continue; }
        if (!strcmp(arg,"--stress-probe")) { o->stress=1; o->probe=1; o->frames=720; continue; }
        if (!strcmp(arg,"--no-audio")) { o->no_audio=1; continue; }
        if (!strcmp(arg,"--autoplay")) { o->autoplay=1; continue; }
        if (i+1>=argc) return 0;
        if (!strcmp(arg,"--data")) o->data=argv[++i];
        else if (!strcmp(arg,"--assets")) o->assets=argv[++i];
        else if (!strcmp(arg,"--save-dir")) o->save_dir=argv[++i];
        else if (!strcmp(arg,"--screenshot")) o->screenshot=argv[++i];
        else if (!strcmp(arg,"--screen")) o->screen=argv[++i];
        else if (!strcmp(arg, "--seed")) {
            if (!number(argv[++i], 0, INT_MAX, &o->seed))
                return 0;
        } else if (!strcmp(arg, "--days")) {
            if (!number(argv[++i], 1, 10000, &o->days))
                return 0;
        } else if (!strcmp(arg, "--frames")) {
            if (!number(argv[++i], 1, 1000000, &o->frames))
                return 0;
        } else if (!strcmp(arg, "--stress-animals")) {
            if (!number(argv[++i], 180, 300, &o->wildlife) || (o->wildlife != 180 && o->wildlife != 300))
                return 0;
        } else if (!strcmp(arg, "--width")) {
            if (!number(argv[++i], 1024, 3840, &o->width))
                return 0;
        } else if (!strcmp(arg, "--height")) {
            if (!number(argv[++i], 720, 2160, &o->height))
                return 0;
        } else if (!strcmp(arg, "--difficulty")) {
            const char *d=argv[++i];
            if (!strcmp(d,"peaceful")) o->difficulty=0;
            else if (!strcmp(d,"normal")) o->difficulty=1;
            else if (!strcmp(d,"harsh")) o->difficulty=2;
            else if (!number(d,0,2,&o->difficulty)) return 0;
        } else
            return 0;
    }
    if (strcmp(o->screen,"title") && strcmp(o->screen,"game") && strcmp(o->screen,"villagers") && strcmp(o->screen,"population") && strcmp(o->screen,"rotations") && strcmp(o->screen,"connections") && strcmp(o->screen,"research") && strcmp(o->screen,"rank") && strcmp(o->screen,"market") && strcmp(o->screen,"load")) return 0;
    return 1;
}
void game_app_notice(GameApp *a,const char *text)
{
    GameNotice *n; size_t length=strlen(text);
    if (a->notice_count==8) { memmove(a->notices,a->notices+1,7*sizeof(*a->notices)); a->notice_count--; }
    n=&a->notices[a->notice_count++]; memset(n,0,sizeof(*n));
    if (length>=sizeof(n->text)) { length=sizeof(n->text)-1; while (length>0 && ((unsigned char)text[length]&0xC0)==0x80) length--; }
    memcpy(n->text,text,length);
}
void game_app_result(GameApp *a,int result,int reason,int placement)
{
    const char *message=result==CORE_ERR_REJECTED?(placement?fmt_place_result(reason):fmt_deny_reason(reason)):fmt_result_message(result);
    (void)snprintf(a->message,sizeof(a->message),"%s",message);
    if (result<0) game_sound(&a->assets,SND_DENIED,a->ui.volume);
}
void game_app_open(GameApp *a,GamePanel panel)
{
    if (a->panel==PANEL_NONE) a->saved_speed=a->speed;
    if (a->panel_depth<8) a->panel_stack[a->panel_depth++]=a->panel;
    a->return_panel=a->panel; a->panel=panel; a->speed=0; a->scroll=0; a->ui.focus=0; a->dragging=0;
}
void game_app_close(GameApp *a)
{
    if (a->panel==PANEL_TITLE) return;
    if (a->panel==PANEL_SETTINGS && !a->automated && !game_settings_save(&a->settings,a->settings_path)) game_app_notice(a,u8"설정 파일을 저장하지 못했습니다.");
    a->panel=a->panel_depth>0?a->panel_stack[--a->panel_depth]:(a->in_game?PANEL_NONE:PANEL_TITLE);
    a->return_panel=a->in_game?PANEL_NONE:PANEL_TITLE;
    if (a->panel==PANEL_NONE) a->speed=a->saved_speed;
    a->scroll=0; a->ui.focus=0;
}
static void enter_game(GameApp *a)
{
    a->in_game=1; a->panel=PANEL_NONE; a->return_panel=PANEL_NONE; a->panel_depth=0; a->speed=1;
    a->selected_building=0; a->selected_villager=0; a->build_type=-1; a->demolish=0; a->dragging=0; a->inspector_scroll=0;
    a->shown_outcome=a->session.summary.outcome==CORE_OUTCOME_WON?CORE_OUTCOME_WON:CORE_OUTCOME_NONE;
    a->camera.target=(Vector2){a->session.summary.keep_x*16.0f,a->session.summary.keep_y*16.0f};
    a->camera.zoom=3; a->accumulator=0; a->autosave_day=a->session.summary.day; a->autosave_pending=0; a->notice_count=0;
}
int game_app_start(GameApp *a)
{
    int seed=(int)((unsigned long)time(NULL)&0x7FFFFFFFul),rc;
    if (a->seed_text[0] && !number(a->seed_text,0,INT_MAX,&seed)) { game_app_notice(a,u8"지도 시드는 0~2147483647의 정수여야 합니다."); return 0; }
    rc=game_session_new(&a->session,a->castle_name,seed,a->difficulty);
    if (rc!=CORE_OK) { game_app_result(a,rc,0,0); return 0; }
    enter_game(a); game_session_events(&a->session);
    if (!a->automated && !a->stress) (void)game_app_save(a,0);
    return 1;
}
void game_app_slot_path(GameApp *a,int slot,int backup,char path[GAME_PATH_BYTES])
{
    char name[48];
    if (slot==0) (void)snprintf(name,sizeof(name),"autosave.sav%s",backup?".bak":"");
    else (void)snprintf(name,sizeof(name),"slot%d.sav%s",slot,backup?".bak":"");
    if (!game_join_path(path,GAME_PATH_BYTES,a->save_dir,name)) path[0]='\0';
}
int game_app_save(GameApp *a,int slot)
{
    char path[GAME_PATH_BYTES]; int rc;
    if (!a->in_game || a->stress || a->session.summary.outcome==CORE_OUTCOME_LOST) return CORE_IGNORED;
    if (!game_make_dirs(a->save_dir)) { game_app_result(a,CORE_ERR_FILE_IO,0,0); return CORE_ERR_FILE_IO; }
    game_app_slot_path(a,slot,0,path); rc=core_save(a->session.world,path);
    if (slot==0 && rc==CORE_ERR_REJECTED) a->autosave_pending=1;
    if (rc==CORE_OK) {
        if (slot==0) a->autosave_pending=0;
        a->save_count++; (void)snprintf(a->message,sizeof(a->message),u8"%s 저장했습니다.",slot?u8"슬롯에":u8"자동으로");
    } else game_app_result(a,rc,0,0);
    return rc;
}
int game_app_load(GameApp *a,int slot,int backup)
{
    char path[GAME_PATH_BYTES]; int rc,line=0;
    game_app_slot_path(a,slot,backup,path); rc=game_session_load(&a->session,path,&line);
    if (rc==CORE_OK) { enter_game(a); game_app_notice(a,u8"저장된 성채를 불러왔습니다."); return 1; }
    if (!backup && (rc==CORE_ERR_FORMAT || rc==CORE_ERR_VERSION || rc==CORE_ERR_FILE_OPEN)) {
        CoreSaveInfoView info;
        game_app_slot_path(a,slot,1,path);
        if (core_peek_save(path,a->session.defs,&info)==CORE_OK) {
            a->backup_slot=slot; a->confirm_action=3;
            (void)snprintf(a->confirm_text,sizeof(a->confirm_text),u8"저장 파일을 읽지 못했습니다 (%s, %d번째 줄).\n이전 저장인 예비 파일(.bak)로 불러올까요?",fmt_result_message(rc),line);
            game_app_open(a,PANEL_CONFIRM); return 0;
        }
    }
    (void)snprintf(a->message,sizeof(a->message),u8"불러오기 실패: %s (%d번째 줄)",fmt_result_message(rc),line); return 0;
}
int game_app_latest(GameApp *a)
{
    int slot,best=-1,best_tick=-1; unsigned long long newest=0;
    for (slot=0;slot<=3;slot++) {
        char path[GAME_PATH_BYTES]; CoreSaveInfoView info; unsigned long long modified;
        game_app_slot_path(a,slot,0,path); modified=game_file_modified(path);
        if (core_peek_save(path,a->session.defs,&info)!=CORE_OK) {
            game_app_slot_path(a,slot,1,path);
            if (core_peek_save(path,a->session.defs,&info)!=CORE_OK) continue;
            if (!modified) modified=game_file_modified(path);
        }
        if (best<0 || modified>newest || (modified==newest && info.tick>best_tick)) { best=slot; newest=modified; best_tick=info.tick; }
    }
    return best;
}
static void event(void *context,const CoreEventView *e)
{
    GameApp *a=(GameApp *)context; char text[512]; int sound=-1;
    if (!a->in_game) return;
    switch (e->type) {
    case CORE_EVT_PRODUCED: case CORE_EVT_DAY_STARTED: case CORE_EVT_NIGHT_FELL: case CORE_EVT_DAWN: return;
    case CORE_EVT_THREAT_NEAR: case CORE_EVT_RAID_STARTED: sound=SND_ALARM; break;
    case CORE_EVT_BUILDING_COMPLETED: sound=SND_DONE; break;
    case CORE_EVT_RESEARCH_DONE: case CORE_EVT_RANK_UP: case CORE_EVT_VICTORY: case CORE_EVT_TUTORIAL_DONE: sound=SND_FANFARE; break;
    default: break;
    }
    if(a->session.summary.ruleset && (e->type==CORE_EVT_RANK_UP || e->type==CORE_EVT_RANK_READY || e->type==CORE_EVT_RESEARCH_STARTED || e->type==CORE_EVT_RESEARCH_DONE || e->type==CORE_EVT_TUTORIAL_STEP || e->type==CORE_EVT_TUTORIAL_DONE))text[0]='\0';
    else fmt_event(e,text,sizeof(text));
    if(a->session.summary.ruleset){
        if(e->type==CORE_EVT_RANK_UP)(void)snprintf(text,sizeof(text),u8"영지가 %s(으)로 승급했습니다.",core_expansion_rank_name(e->a));
        else if(e->type==CORE_EVT_RANK_READY)(void)snprintf(text,sizeof(text),u8"%s 승급 조건을 확인하세요.",core_expansion_rank_name(e->a));
        else if(e->type==CORE_EVT_RESEARCH_STARTED || e->type==CORE_EVT_RESEARCH_DONE)(void)snprintf(text,sizeof(text),u8"%s 연구를 %s.",core_expansion_research_name(e->a),e->type==CORE_EVT_RESEARCH_DONE?u8"완료했습니다":u8"시작했습니다");
        else if(e->type==CORE_EVT_TUTORIAL_STEP)(void)snprintf(text,sizeof(text),u8"기초 목표 완료: %s",core_expansion_goal_name(e->a));
        else if(e->type==CORE_EVT_TUTORIAL_DONE)(void)snprintf(text,sizeof(text),u8"기초 목표를 모두 마쳤습니다. 다음 등급에 도전하세요.");
    }
    if (text[0]) {
        game_app_notice(a,text);
        if (e->type==CORE_EVT_BUILDING_COMPLETED || e->type==CORE_EVT_BUILDING_PLACED || e->type==CORE_EVT_BUILDING_RUINED) {
            GameNotice *n=&a->notices[a->notice_count-1]; n->x=e->c; n->y=e->d; n->has_position=1;
        }
    }
    if (sound>=0) game_sound(&a->assets,sound,a->settings.sfx_volume);
}
static int smoke(GameApp *a,const Options *o)
{
    int day=0,rc=CORE_OK,target=o->days*144,i; uint64_t before=0,after=0;
    char path[GAME_PATH_BYTES]; unsigned long arrivals,shots,kills;
    a->in_game=1;
    rc=game_session_new(&a->session,u8"자동 검사 성채",o->seed,o->difficulty);
    if (rc!=CORE_OK) return 1;
    for (i=0;i<target;i++) {
        if (day!=a->session.summary.day) {
            day=a->session.summary.day; (void)sim_bot_daily(a->session.world,"reasonable");
        }
        rc=game_session_step(&a->session,10);
        if (rc!=CORE_OK || core_validate(a->session.world)!=0 || a->session.summary.outcome==CORE_OUTCOME_LOST) {
            fprintf(stderr,"SMOKE FAILED: day=%d result=%d population=%d\n",a->session.summary.day,rc,a->session.summary.population); return 1;
        }
        if (a->autosave_day!=a->session.summary.day) { a->autosave_day=a->session.summary.day; (void)game_app_save(a,0); }
        if (a->autosave_pending && !a->session.summary.raid_id) (void)game_app_save(a,0);
    }
    for (i=0;a->session.summary.raid_id && i<150;i++) if (game_session_step(&a->session,10)!=CORE_OK) return 1;
    if (!game_make_dirs(a->save_dir) || !game_join_path(path,sizeof(path),a->save_dir,"roundtrip.sav")) return 1;
    arrivals=a->session.arrivals; shots=a->session.shots; kills=a->session.kills;
    if (core_state_hash(a->session.world,&before)!=CORE_OK || core_save(a->session.world,path)!=CORE_OK ||
        game_session_load(&a->session,path,NULL)!=CORE_OK || core_state_hash(a->session.world,&after)!=CORE_OK || before!=after || core_validate(a->session.world)!=0) return 1;
    printf("SMOKE PASS: days=%d population=%d rank=%d research=%d arrivals=%lu shots=%lu kills=%lu saves=%d hash=%llu\n",
        o->days,a->session.summary.population,a->session.summary.rank,a->session.summary.research_done,arrivals,shots,kills,a->save_count,(unsigned long long)after);
    return 0;
}
static void screenshot_screen(GameApp *a,const Options *o)
{
    if (!strcmp(o->screen,"title")) { a->panel=PANEL_TITLE; a->in_game=0; return; }
    (void)game_session_new(&a->session,u8"바람언덕",o->seed,o->difficulty); enter_game(a);
    if(!strcmp(o->screen,"villagers") || !strcmp(o->screen,"rotations") || !strcmp(o->screen,"connections")){a->speed=0;return;}
    {
        int day;
        for(day=0;day<16;day++){
            (void)sim_expansion_bot_daily(a->session.world);
            (void)game_session_step(&a->session,1440);
        }
        {
            int x,y,placed=0;int32_t id=0,reason=0;
            for(y=a->session.summary.keep_y-7;y<=a->session.summary.keep_y+7 && !placed;y++)
                for(x=a->session.summary.keep_x-7;x<=a->session.summary.keep_x+7 && !placed;x++){
                    CorePlacementView p;
                    (void)core_can_place_rotated(a->session.world,CORE_BLD_GARDEN,x,y,0,&p);
                    if(p.result==CORE_PLACE_OK && core_place_rotated(a->session.world,CORE_BLD_GARDEN,x,y,0,&id,&reason)==CORE_OK){
                        (void)core_set_auto_assign(a->session.world,id,0);placed=1;
                    }
                }
            (void)game_session_step(&a->session,360);
            (void)game_session_step(&a->session,(float)((660-a->session.summary.minute_of_day+1440)%1440));
        }
        (void)game_session_refresh(&a->session);
    }
    game_session_events(&a->session); a->notice_count=0; a->message[0]='\0';
    if(!strcmp(o->screen,"game")){
        int b,v;
        for(b=0;b<a->session.building_count;b++)if(a->session.buildings[b].type==CORE_BLD_GARDEN){
            int id=a->session.buildings[b].id;
            (void)core_set_auto_assign(a->session.world,id,0);
            for(v=0;v<a->session.villager_count;v++)if(a->session.villagers[v].workplace_id==id)
                (void)core_unassign_worker(a->session.world,a->session.villagers[v].id);
            break;
        }
        (void)game_session_refresh(&a->session);
    }
    a->speed=0;
    if (!strcmp(o->screen,"research")) a->panel=PANEL_RESEARCH;
    else if (!strcmp(o->screen,"population")) a->panel=PANEL_VILLAGERS;
    else if (!strcmp(o->screen,"rank")) a->panel=PANEL_RANK;
    else if (!strcmp(o->screen,"market")) a->panel=PANEL_MARKET;
    else if (!strcmp(o->screen,"load")) a->panel=PANEL_LOAD;
}
static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left, b = *(const double *)right;
    return (a > b) - (a < b);
}
static int run(int argc,char **argv)
{
    Options o; GameApp *a; char exe[GAME_PATH_BYTES],data[GAME_PATH_BYTES],assets[GAME_PATH_BYTES];
    int status=0,frame=0,last_bot_day=-1; double frame_total=0,frame_max=0;
    double samples[600], sim_total = 0, render_total = 0;
    int measured = 0, min_villagers = 100000, min_enemies = 100000, min_wild = 100000;
    if (argc>1 && (!strcmp(argv[1],"--help") || !strcmp(argv[1],"-h"))) { help(); return 0; }
    if (!options(argc,argv,&o)) { help(); return 2; }
    if (!game_executable_directory(exe,sizeof(exe),argv[0]) || !game_join_path(data,sizeof(data),exe,"data") || !game_join_path(assets,sizeof(assets),exe,"assets")) return 1;
    a=(GameApp *)calloc(1,sizeof(*a)); if (!a) return 1;
    a->automated=o.smoke || o.screenshot || o.frames || o.probe; a->stress=o.stress; a->build_type=-1; a->category=CORE_CAT_AGRICULTURE;
    a->difficulty=o.difficulty; strcpy(a->castle_name,u8"바람언덕");
    if (a->automated || o.stress) (void)snprintf(a->seed_text,sizeof(a->seed_text),"%d",o.seed);
    if (!game_user_directory(a->user_dir,sizeof(a->user_dir))) { free(a); return 1; }
    if (o.save_dir) {
        if (strlen(o.save_dir)>=sizeof(a->save_dir)) { free(a); return 2; }
        strcpy(a->save_dir,o.save_dir);
    } else if (!game_join_path(a->save_dir, sizeof(a->save_dir), a->automated ? exe : a->user_dir,
                               a->automated ? "saves_v8_automated" : "saves_v8")) {
        free(a);
        return 1;
    }
    if (!game_join_path(a->settings_path,sizeof(a->settings_path),a->user_dir,"settings.cfg")) { free(a); return 1; }
    status=game_session_init(&a->session,o.data?o.data:data);
    a->session.ruleset=CORE_RULES_EXPANSION;
    if (status!=CORE_OK) { fprintf(stderr,"C core initialization failed: %d\n",status); free(a); return 1; }
    a->session.on_event=event; a->session.event_context=a;
    game_settings_default(&a->settings);
    if (o.smoke) { status=smoke(a,&o); game_session_free(&a->session); free(a); return status; }
    if (!a->automated) (void)game_settings_load(&a->settings,a->settings_path);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | (o.probe ? 0 : FLAG_VSYNC_HINT));
    InitWindow(o.width, o.height, u8"마법성채 키우기 — C");
    SetWindowMinSize(1024, 720);
    SetExitKey(KEY_NULL);
    SetTargetFPS(o.probe ? 0 : 60);
    if (!IsWindowReady() || !game_assets_load(&a->assets,o.assets?o.assets:assets,!o.no_audio)) {
        fprintf(stderr,u8"게임 리소스를 읽지 못했습니다. --assets로 assets 폴더를 지정하세요.\n");
        game_assets_free(&a->assets); if (IsWindowReady()) CloseWindow(); game_session_free(&a->session); free(a); return 1;
    }
    if (a->settings.fullscreen) ToggleFullscreen();
    if (a->assets.audio_ready) SetMasterVolume(a->settings.master_volume);
    a->camera=(Camera2D){{o.width*0.5f-160,o.height*0.5f},{48*16,48*16},0,3};
    (void)game_session_new(&a->session,u8"바람언덕",42,1); a->panel=PANEL_TITLE;
    if (o.stress || (o.frames && !o.screenshot)) {
        (void)game_app_start(a);
        if (o.stress) {
            (void)core_debug_stress(a->session.world, 150, 40);
            if (o.wildlife == 300)
                (void)core_expansion_debug_wildlife(a->session.world, 300);
            (void)game_session_refresh(&a->session);
            a->camera.zoom = 2;
        }
    }
    if (o.screenshot) screenshot_screen(a,&o);
    while (!a->quit) {
        double frame_started = GetTime(), sim_ms = 0, render_started = 0, render_ms = 0;
        float delta = o.probe ? 1.0f / 60 : fminf(GetFrameTime(), 0.1f);
        int i;
        unsigned long shots = a->session.shots;
        if (WindowShouldClose()) { if (!a->automated) (void)game_app_save(a,0); break; }
        frame++;
        a->camera.offset=(Vector2){(GetScreenWidth()-350)*0.5f,GetScreenHeight()*0.5f};
        for (i=0;i<a->notice_count;i++) a->notices[i].age+=delta;
        if (a->panel!=PANEL_NONE && a->panel!=PANEL_TITLE && IsKeyPressed(KEY_ESCAPE)) game_app_close(a);
        else game_world_input(a,delta);
        if (o.autoplay && a->in_game && a->session.summary.day!=last_bot_day) { last_bot_day=a->session.summary.day; (void)sim_bot_daily(a->session.world,"reasonable"); }
        if(o.autoplay && a->in_game && a->session.summary.rank_ready) {int32_t why=0;(void)core_rank_up(a->session.world,&why);}
        if (a->in_game && a->panel==PANEL_NONE && a->speed>0 && a->session.summary.outcome!=CORE_OUTCOME_LOST) {
            float minutes=a->session.summary.ruleset?delta*a->speed*12.0f:delta*a->speed*1000/a->session.config.real_ms_per_tick*a->session.config.minutes_per_tick;
            double sim_started = GetTime();
            status=game_session_step(&a->session,minutes);
            sim_ms = (GetTime() - sim_started) * 1000;
            if (status!=CORE_OK && status!=CORE_ERR_REJECTED) { game_app_result(a,status,0,0); a->speed=0; }
            if (!a->automated && !a->stress && a->session.summary.day!=a->autosave_day) {
                a->autosave_day=a->session.summary.day; (void)game_app_save(a,0);
            }
            if (a->autosave_pending && !a->session.summary.raid_id) (void)game_app_save(a,0);
        }
        if (a->session.shots!=shots) game_sound(&a->assets,SND_ARROW,a->settings.sfx_volume*0.25f);
        if (a->in_game && a->session.summary.outcome && a->session.summary.outcome!=a->shown_outcome && !a->automated) {
            a->shown_outcome=a->session.summary.outcome; game_app_open(a,PANEL_OUTCOME);
        }
        ui_begin(&a->ui,&a->assets,a->settings.ui_scale==2?24.0f:20.0f,a->settings.sfx_volume);
        render_started = GetTime();
        BeginDrawing(); ClearBackground(UI_BG);
        if (o.screenshot && !strcmp(o.screen,"villagers")) game_render_villager_preview(a);
        else if (o.screenshot && !strcmp(o.screen,"rotations")) game_render_rotation_preview(a);
        else if (o.screenshot && !strcmp(o.screen,"connections")) game_render_connection_preview(a);
        else { game_render_world(a); game_render_interface(a); }
        EndDrawing();
        render_ms = (GetTime() - render_started) * 1000;
        /* 일시정지 중의 명령도 다음 프레임 전에 스냅숏에 반영합니다. */
        if (game_session_refresh(&a->session)!=CORE_OK) { status=1; break; }
        game_session_events(&a->session);
        if (o.probe && frame > 120 && measured < 600) {
            double ms = (GetTime() - frame_started) * 1000;
            samples[measured++] = ms;
            frame_total += ms;
            sim_total += sim_ms;
            render_total += render_ms;
            if (ms > frame_max)
                frame_max = ms;
            if (a->session.villager_count < min_villagers)
                min_villagers = a->session.villager_count;
            int active_enemies = 0;
            for (int n = 0; n < a->session.enemy_count; n++)
                active_enemies += a->session.enemies[n].hp > 0;
            if (active_enemies < min_enemies)
                min_enemies = active_enemies;
            if (a->session.summary.wild_count < min_wild)
                min_wild = a->session.summary.wild_count;
        }
        if (o.screenshot && frame==4) { status=game_screenshot(o.screenshot)?0:1; break; }
        if (o.frames && frame>=o.frames) break;
    }
    if (o.probe) {
        qsort(samples, (size_t)measured, sizeof(double), compare_double);
        double avg = measured ? frame_total / measured : 0,
               p95 = measured ? samples[(measured - 1) * 95 / 100] : 0,
               p99 = measured ? samples[(measured - 1) * 99 / 100] : 0;
        int conditions =
            measured == 600 && min_villagers >= 150 && min_enemies >= 40 && min_wild >= o.wildlife;
        int passed = conditions && (o.wildlife == 300 || (avg <= 16.67 && p95 <= 20 && p99 <= 33.3));
        printf("STRESS: warmup=120 frames=%d avg_ms=%.3f p95_ms=%.3f p99_ms=%.3f max_ms=%.3f sim_ms=%.3f "
               "render_ms=%.3f min_villagers=%d min_enemies=%d min_wild=%d livestock=%d mode=%s "
               "conditions=%d passed=%d\n",
               measured, avg, p95, p99, frame_max, measured ? sim_total / measured : 0,
               measured ? render_total / measured : 0, min_villagers, min_enemies, min_wild,
               a->session.summary.animal_count - a->session.summary.wild_count,
               o.wildlife == 300 ? "debug300" : "normal180", conditions, passed);
        if (!passed)
            status = 1;
    }
    if (!a->automated) { (void)game_make_dirs(a->user_dir); (void)game_settings_save(&a->settings,a->settings_path); }
    game_render_free(a); game_assets_free(&a->assets); CloseWindow(); game_session_free(&a->session); free(a);
    return status<0?1:status;
}

int main(int argc,char **argv)
{
    GameArguments args; int result;
    if (!game_arguments(&args,argc,argv)) return 1;
    result=run(args.count,args.values); game_arguments_free(&args); return result;
}
