/* combat.c — 적·경비병·감시탑·투사체. 규칙의 적용은 반드시 공개 보고 API를 거칩니다. */
#include "session.h"
#include "sprite_layout.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { TARGET_NONE = 0, TARGET_ENEMY = 1, TARGET_BUILDING = 2, TARGET_VILLAGER = 3, TARGET_ANIMAL = 4 };
static int researched(const GameSession *s, int index) {
    CoreResearchView view;
    return core_get_research(s->world, index, &view) == CORE_OK && view.state == CORE_RSTATE_DONE;
}
void game_combat_clear(GameSession *s) {
    int i;
    for (i = 0; i < s->enemy_count; i++)
        game_path_free(&s->enemies[i].actor.path);
    s->enemy_count = 0;
    s->projectile_count = 0;
    s->raid_id = 0;
    s->tower_count = 0;
    memset(&s->raid, 0, sizeof(s->raid));
}
static void spread_entry(GameSession *s,GameEnemy *e,int index) {
    int w=s->summary.map_width,h=s->summary.map_height;
    int vertical=s->raid.spawn_x==0||s->raid.spawn_x==w-1;
    int along=vertical?s->raid.spawn_y:s->raid.spawn_x;
    for(int attempt=0;attempt<25;attempt++) {
        int offset=((index*7+attempt*5)%25)-12;
        int v=along+offset;if(v<1||v>=(vertical?h:w)-1)continue;
        int x=vertical?s->raid.spawn_x:v,y=vertical?v:s->raid.spawn_y;
        if(!game_path_cost(s->cost[y*w+x],1))continue;
        GamePoint original=e->actor.position;
        e->actor.position=(GamePoint){x+.5f,y+.5f};
        int keep=game_building_at(s,s->summary.keep_x,s->summary.keep_y);CoreBuildingView *manor=game_building(s,keep);
        if(manor && game_actor_plan(s,&e->actor,manor->door_x,manor->door_y,1)<=0){e->actor.position=original;continue;}
        e->entry_x=x;e->entry_y=y;break;
    }
    e->pending_spawn=index>0;e->retarget=index*4.0f;
}
int game_combat_sync(GameSession *s) {
    CoreEnemyView *views = NULL;
    int32_t count = 0;
    int i, rc;
    rc = core_get_raid(s->world, &s->raid);
    if (rc != CORE_OK)
        return rc;
    if (s->raid.id != s->raid_id) {
        CoreRaidView raid = s->raid;
        game_combat_clear(s);
        for(int k=0;k<2048;k++)if(s->effects[k].category==2)memset(&s->effects[k],0,sizeof(s->effects[k]));
        s->raid = raid;
        s->raid_id = raid.id;

    }
    if (!s->raid_id)
        return CORE_OK;
    count = s->raid.enemy_count;
    views = malloc((size_t)count * sizeof(*views));
    if (!views) return CORE_ERR_OUT_OF_MEMORY;
    if (count > s->enemy_capacity) {
        int capacity = count < 64 ? 64 : count * 2;
        GameEnemy *grown = realloc(s->enemies,(size_t)capacity * sizeof(*grown));
        if (!grown) {free(views);return CORE_ERR_OUT_OF_MEMORY;}
        s->enemies = grown; s->enemy_capacity = capacity;
    }
    rc = core_copy_enemies(s->world, views, count, &count);
    if (rc != CORE_OK) {free(views);return rc;}
    for (i = 0; i < count; i++) {
        int j;
        GameEnemy *enemy = NULL;
        for (j = 0; j < s->enemy_count; j++)
            if (s->enemies[j].actor.id == views[i].id) {
                enemy = &s->enemies[j];
                break;
            }
        if (views[i].state != CORE_ENEMY_ALIVE) {
            if (enemy)
                enemy->hp = 0;
            continue;
        }
        if (!enemy) {

            enemy = &s->enemies[s->enemy_count++];
            memset(enemy, 0, sizeof(*enemy));
            rc = core_get_enemy_def(s->world, views[i].type, &enemy->def);
            if (rc != CORE_OK) {free(views);return rc;}
            enemy->summoner_id = views[i].summoner_id;
            if (s->summary.ruleset) (void)core_expansion_raider_info(views[i].type,&enemy->info);
            if (enemy->summoner_id) {enemy->def.hp=40;enemy->def.damage=5;}
            enemy->actor.id = views[i].id;
            enemy->hp = enemy->def.hp;
            enemy->actor.grid_revision = -1;
            enemy->actor.position = (GamePoint){s->raid.spawn_x + 0.5f, s->raid.spawn_y + 0.5f};
            enemy->entry_x=s->raid.spawn_x;enemy->entry_y=s->raid.spawn_y;
            if(s->summary.ruleset && !enemy->summoner_id)spread_entry(s,enemy,i);
            if (enemy->summoner_id) {
                GameEnemy *parent = game_enemy(s,enemy->summoner_id);
                if (parent) enemy->actor.position=parent->actor.position;
            }
            const CoreEnemyRuntime *r=&views[i].runtime;
            if(r->valid) {
                enemy->pending_spawn=r->valid==2;
                enemy->hp=r->hp;enemy->actor.position=(GamePoint){r->x_milli/1000.0f,r->y_milli/1000.0f};
                enemy->entry_x=r->entry_x;enemy->entry_y=r->entry_y;
                enemy->actor.cooldown=r->cooldown_milli/1000.0f;enemy->special_clock=r->special_milli/1000.0f;
                enemy->attack_elapsed=r->attack_elapsed_milli/1000.0f;enemy->attack_serial=r->attack_serial;
                enemy->attack_applied=r->attack_applied;enemy->attack_kind=r->attack_kind;enemy->attack_target=r->attack_target;
                enemy->attack_to=(GamePoint){r->attack_x_milli/1000.0f,r->attack_y_milli/1000.0f};
                enemy->retarget=r->retarget_milli/1000.0f;enemy->fleeing=r->fleeing;enemy->retreat_index=r->retreat_index;enemy->inbound_count=r->inbound_count;
                memcpy(enemy->inbound,r->inbound,sizeof(enemy->inbound));enemy->retreat_started=enemy->fleeing;
            }
        }
    }
    free(views);
    return CORE_OK;
}
static int guard(const CoreVillagerView *v) {
    return v->state == CORE_VS_GUARD_POST && !v->inside_building && v->health > 0;
}
static int exposed(const GameSession *s,const CoreVillagerView *v) {
    return v->health > 0 && !v->inside_building && !v->incoming && v->state != CORE_VS_LEAVING &&
           (s->summary.ruleset || !(v->state == CORE_VS_FLEE && v->at_target));
}
static GamePoint center(const CoreBuildingView *b) {
    return (GamePoint){b->x + b->w * 0.5f, b->y + b->h * 0.5f};
}
static float building_radius(GamePoint p,const CoreBuildingView *b) {
    int x=(int)p.x,y=(int)p.y,lx=b->x+(b->w-1)/2,rx=b->x+b->w/2,ty=b->y+(b->h-1)/2,by=b->y+b->h/2;
    return (float)((x<lx?lx-x:x>rx?x-rx:0)+(y<ty?ty-y:y>by?y-by:0));
}
static float building_distance(GamePoint p, const CoreBuildingView *b) {
    float dx = fmaxf(fmaxf(b->x - p.x, 0), p.x - (b->x + b->w));
    float dy = fmaxf(fmaxf(b->y - p.y, 0), p.y - (b->y + b->h));
    return sqrtf(dx * dx + dy * dy);
}
static void hostile_area(GameSession *s,GamePoint p,float radius,int damage,int exclude_kind,int exclude_id,int source_id);
static void hurt_enemy(GameSession *s, int id, int damage,int style,GamePoint from) {
    GameEnemy *e = game_enemy(s, id);
    if (!e)
        return;
    e->hp -= damage;
    game_hit_fx_direction(s,from,e->actor.position,style);
    if (e->hp <= 0) {
        e->hp = 0;
        if(e->info.ability==9 && !e->summoner_id)hostile_area(s,e->actor.position,1.5f,10,TARGET_NONE,0,e->actor.id);
        if (core_report_enemy_killed(s->world, s->raid_id, id) == CORE_OK)
            s->kills++;
    }
}
static void impact(GameSession *s, const GameProjectile *p) {
    if (p->target_kind!=TARGET_ANIMAL && p->raid_id != s->raid_id)
        return;
    if (p->target_kind == TARGET_ENEMY) {
        GameEnemy *e = game_enemy(s, p->target_id);
        GamePoint where = e ? e->actor.position : p->to;
        int i;
        if (e) {
            e->incoming -= p->damage;
            if (e->incoming < 0)
                e->incoming = 0;
        }
        hurt_enemy(s, p->target_id, p->damage,game_hit_style(p->source_type),p->from);
        if (p->radius > 0)
            for (i = 0; i < s->enemy_count; i++)
                if (s->enemies[i].actor.id != p->target_id && s->enemies[i].hp > 0 &&
                    game_distance(where, s->enemies[i].actor.position) <= p->radius)
                    hurt_enemy(s, s->enemies[i].actor.id, p->damage,game_hit_style(p->source_type),p->from);
    } else if (p->target_kind == TARGET_BUILDING) {
        CoreBuildingView *b=game_building(s,p->target_id);
        int hit=s->summary.ruleset?core_expansion_building_hit(s->world,p->target_id,p->damage,1,p->source_id):core_report_building_damage(s->world,p->raid_id,p->target_id,p->damage);
        if(hit==CORE_OK && b && b->hp>0)game_hit_fx_direction(s,p->from,center(b),game_hit_style(p->source_type));
        if(p->radius>0 && p->source_type==27)hostile_area(s,p->to,p->radius,p->damage,TARGET_BUILDING,p->target_id,p->source_id);
    } else if (p->target_kind == TARGET_VILLAGER) {
        int applied=s->summary.ruleset?core_expansion_person_hit(s->world,p->target_id,p->damage,1,p->source_id):core_report_villager_damage(s->world,p->raid_id,p->target_id,p->damage);
        if(applied==CORE_OK){GameActor *a=game_actor(s,p->target_id);if(a)game_hit_fx_direction(s,p->from,a->position,game_hit_style(p->source_type));}
        if(applied==CORE_OK && p->source_type==21)(void)core_expansion_slow(s->world,p->target_id);
        if(p->radius>0 && p->source_type==27)hostile_area(s,p->to,p->radius,p->damage,TARGET_VILLAGER,p->target_id,p->source_id);
    } else if (p->target_kind == TARGET_ANIMAL) {
        int i;
        if (core_expansion_hit_animal(s->world,p->target_id,p->damage,p->source_type>=100?1:2,p->source_id) == CORE_OK)
            for (i = 0; i < s->animal_count; i++)
                if (s->animals[i].id == p->target_id) {
                    game_hit_fx_direction(s,p->from,(GamePoint){s->animals[i].x_milli/1000.0f,s->animals[i].y_milli/1000.0f},game_hit_style(p->source_type));
                    s->animals[i].hp -= p->damage;
                    if (s->animals[i].hp <= 0)
                        s->animals[i].alive = 0;
                    break;
                }
    }
}
static void fire(GameSession *s, GamePoint from, GamePoint to, int kind, int id, int damage, float radius,int source,int source_id) {
    GameProjectile p = {0};
    p.source_type = source;
    p.source_id=source_id;
    p.from = from;
    p.to = to;
    p.elapsed = 0;
    p.duration = fmaxf(0.5f, game_distance(from, to));
    p.raid_id = s->raid_id;
    p.target_kind = kind;
    p.target_id = id;
    p.damage = damage;
    p.radius = radius;
    if (s->projectile_count == GAME_MAX_PROJECTILES) {
        impact(s, &p);
        return;
    }
    s->projectiles[s->projectile_count++] = p;
    s->shots++;
    if (kind == TARGET_ENEMY) {
        GameEnemy *e = game_enemy(s, id);
        if (e)
            e->incoming += damage;
    }
}
static void projectiles_step(GameSession *s, float minutes) {
    int i = 0;
    while (i < s->projectile_count) {
        GameProjectile *p = &s->projectiles[i];
        p->elapsed += minutes;
        if (p->elapsed >= p->duration) {
            GameProjectile finished = *p;
            s->projectiles[i] = s->projectiles[--s->projectile_count];
            impact(s, &finished);
        } else
            i++;
    }
}
static GameEnemy *nearest_enemy(GameSession *s, GamePoint position, float range, int incoming,const CoreBuildingView *source) {
    GameEnemy *best = NULL;
    int i;
    for (i = 0; i < s->enemy_count; i++) {
        GameEnemy *e = &s->enemies[i];
        float distance;
        if (e->hp <= 0 || e->pending_spawn || (incoming && e->incoming >= e->hp))
            continue;
        if (s->visibility) {
            int x = (int)e->actor.position.x, y = (int)e->actor.position.y;
            if (x < 0 || y < 0 || x >= s->summary.map_width || y >= s->summary.map_height ||
                s->visibility[y * s->summary.map_width + x] < 2)
                continue;
        }
        distance = source?building_radius(e->actor.position,source):game_distance(position, e->actor.position);
        if (distance <= range) {
            range = distance;
            best = e;
        }
    }
    return best;
}
static CoreAnimalView *nearest_animal(GameSession *s, GamePoint position, float range,const CoreBuildingView *source) {
    CoreAnimalView *best = NULL;
    int i;
    if (!s->summary.ruleset || !s->visibility)
        return NULL;
    for (i = 0; i < s->animal_count; i++) {
        CoreAnimalView *a = &s->animals[i];
        int x = a->x_milli / 1000, y = a->y_milli / 1000;
        float distance;
        CoreAnimalInfo species;core_expansion_animal_info(a->kind,&species);
        if (!a->alive || a->owner || (species.behavior!=2 && a->threat_until_tick<=s->summary.tick) ||
            s->visibility[y * s->summary.map_width + x] < 2)
            continue;
        GamePoint point={a->x_milli/1000.0f,a->y_milli/1000.0f};
        distance=source?building_radius(point,source):game_distance(position,point);
        if (distance <= range) {
            best = a;
            range = distance;
        }
    }
    return best;
}
static GameTower *tower_state(GameSession *s, int id) {
    int i;
    for (i = 0; i < s->tower_count; i++)
        if (s->towers[i].id == id)
            return &s->towers[i];
    if (s->tower_count == s->tower_capacity) {
        int cap = s->tower_capacity ? s->tower_capacity * 2 : 32;
        GameTower *p = (GameTower *)realloc(s->towers, (size_t)cap * sizeof(*p));
        if (!p) {
            s->last_error = CORE_ERR_OUT_OF_MEMORY;
            return NULL;
        }
        s->towers = p;
        s->tower_capacity = cap;
    }
    s->towers[s->tower_count] = (GameTower){id, 0};
    return &s->towers[s->tower_count++];
}
static CoreAnimalView *hunt_target(GameSession *s,const CoreVillagerView *v,const CoreBuildingView *base,GamePoint position,float range) {
    CoreAnimalView *best=NULL;float nearest=range;
    for(int i=0;i<s->animal_count;i++) {
        CoreAnimalView *a=&s->animals[i];CoreAnimalInfo info;core_expansion_animal_info(a->kind,&info);
        if(!a->alive||a->owner)continue;
        if(v && abs(a->x_milli/1000-(int)position.x)+abs(a->y_milli/1000-(int)position.y)>4)continue;
        float distance=game_distance(position,(GamePoint){a->x_milli/1000.0f,a->y_milli/1000.0f});
        float radius=base->type==CORE_BLD_HUNTER_CAMP?8.0f:15.0f;
        if(building_radius((GamePoint){a->x_milli/1000.0f,a->y_milli/1000.0f},base)>radius)continue;
        if(base->type==CORE_BLD_HUNTER_CAMP&&info.biomass>4 && a->attacker_id!=(v?v->id:base->id))continue;
        int cell=a->y_milli/1000*s->summary.map_width+a->x_milli/1000;
        if(s->visibility&&s->visibility[cell]<2)continue;
        if(distance<=nearest){nearest=distance;best=a;}
    }
    return best;
}
static void towers_step(GameSession *s,float minutes) {
    for(int i=0;i<s->building_count;i++) {
        CoreBuildingView *b=&s->buildings[i];CoreBuildingDefView *d=&s->building_defs[b->type];
        int manor=s->summary.ruleset&&b->type==CORE_BLD_KEEP;
        int hunting=s->summary.ruleset&&(b->type==CORE_BLD_HUNTER_CAMP||b->type==CORE_BLD_HUNTING_LODGE);
        if(b->state!=CORE_BSTATE_ACTIVE)continue;
        if(!manor && (s->summary.ruleset?(!b->working_now||(!hunting&&d->category!=CORE_CAT_TOWERS)):d->attack<=0))continue;
        int damage=manor?(int[]){6,9,13,18,26,35}[s->summary.rank]:hunting?(b->type==CORE_BLD_HUNTER_CAMP?2:4):d->attack;
        int range=manor?4+s->summary.rank:hunting?(b->type==CORE_BLD_HUNTER_CAMP?3:4):d->attack_range;
        int cooldown=manor?25:hunting?(b->type==CORE_BLD_HUNTER_CAMP?30:25):d->attack_cooldown;
        GameTower *state=tower_state(s,b->id);if(!state)return;
        state->cooldown=fmaxf(0,state->cooldown)-minutes;if(state->cooldown>0)continue;
        GameEnemy *enemy=hunting?NULL:nearest_enemy(s,center(b),(float)range,1,b);
        CoreAnimalView *animal=enemy?NULL:hunting?hunt_target(s,NULL,b,center(b),(float)range):nearest_animal(s,center(b),(float)range,b);
        if(!enemy&&!animal)continue;
        if(!hunting&&s->summary.ruleset&&core_expansion_fire(s->world,b->id)!=CORE_OK)continue;
        state->cooldown+=cooldown;
        if(enemy)fire(s,center(b),enemy->actor.position,TARGET_ENEMY,enemy->actor.id,damage,b->type==CORE_BLD_CANNON_TOWER?1.0f:0,100+b->type,b->id);
        else fire(s,center(b),(GamePoint){animal->x_milli/1000.0f,animal->y_milli/1000.0f},TARGET_ANIMAL,animal->id,damage,0,100+b->type,b->id);
    }
}
static void guards_step(GameSession *s,float minutes) {
    for(int i=0;i<s->villager_count;i++) {
        CoreVillagerView *v=&s->villagers[i];CoreBuildingView *base=game_building(s,v->workplace_id);
        GameActor *a=game_actor(s,v->id);
        if(a)a->cooldown=fmaxf(0,a->cooldown)-minutes;
        if(!a||!base||!guard(v)||v->mission==CORE_MISSION_RETURN) {
            if(a){a->chasing=0;a->attack_serial=0;a->attack_applied=1;a->attack_elapsed=4.8f;}
            continue;
        }
        int hunting=base->type==CORE_BLD_HUNTER_CAMP||base->type==CORE_BLD_HUNTING_LODGE;
        if(!hunting&&base->type!=CORE_BLD_BARRACKS&&base->type!=CORE_BLD_GARRISON)continue;
        if(s->summary.ruleset && !a->outfit){a->chasing=0;continue;}
        float sight=4.0f,range=hunting?(base->type==CORE_BLD_HUNTER_CAMP?3.0f:4.0f):1.0f;
        if(a->attack_serial&&a->attack_elapsed<4.8f) {
            a->attack_elapsed+=minutes;
            if(!a->attack_applied&&a->attack_elapsed>=2.4f) {
                a->attack_applied=1;
                int attack_base=hunting?(base->type==CORE_BLD_HUNTER_CAMP?4:6):(base->type==CORE_BLD_GARRISON?13:10);
                int damage=(int)roundf(attack_base*(hunting?.8f+.2f*v->work_efficiency/100.0f:.7f+.3f*v->work_efficiency/100.0f));
                if(a->attack_kind==TARGET_ENEMY) {
                    GameEnemy *e=game_enemy(s,a->attack_target);
                    if(e&&e->hp>0&&game_distance(a->position,e->actor.position)<=range)hurt_enemy(s,e->actor.id,damage,0,a->position);
                } else for(int j=0;j<s->animal_count;j++) {
                    CoreAnimalView *animal=&s->animals[j];if(animal->id!=a->attack_target||!animal->alive)continue;
                    GamePoint to={animal->x_milli/1000.0f,animal->y_milli/1000.0f};
                    if(game_distance(a->position,to)<=range) {
                        if(hunting)fire(s,a->position,to,TARGET_ANIMAL,animal->id,damage,0,50,v->id);
                        else if(core_expansion_hit_animal(s->world,animal->id,damage,2,v->id)==CORE_OK)game_hit_fx_direction(s,a->position,to,0);
                    }
                    break;
                }
            }
            continue;
        }
        GameEnemy *enemy=hunting?NULL:nearest_enemy(s,a->position,s->summary.ruleset?1e9f:sight,0,NULL);
        CoreAnimalView *animal=enemy?NULL:hunting?hunt_target(s,v,base,a->position,sight):nearest_animal(s,a->position,sight,NULL);
        GamePoint goal=enemy?enemy->actor.position:animal?(GamePoint){animal->x_milli/1000.0f,animal->y_milli/1000.0f}:(GamePoint){v->target_x+.5f,v->target_y+.5f};
        int kind=enemy?TARGET_ENEMY:animal?TARGET_ANIMAL:TARGET_NONE,id=enemy?enemy->actor.id:animal?animal->id:0;
        if(!kind || (!hunting && s->dispatch_delay>0)) {a->chasing=0;continue;}
        a->chasing=1;
        a->facing=game_walk_facing(goal.x-a->position.x,goal.y-a->position.y,a->facing);
        if(game_distance(a->position,goal)<=range) {
            if(a->cooldown<=0) {
                a->cooldown+=hunting?(base->type==CORE_BLD_HUNTER_CAMP?30:25):fmaxf(8,roundf(15/(.9f+.1f*v->work_efficiency/100.0f)));
                a->attack_serial++;a->attack_elapsed=0;a->attack_applied=0;a->attack_kind=kind;a->attack_target=id;
            }
        } else {
            int barrier;
            if(a->target_x!=(int)goal.x||a->target_y!=(int)goal.y||a->grid_revision!=s->grid_revision||!a->path.count)
                (void)game_actor_plan(s,a,(int)goal.x,(int)goal.y,0);
            (void)game_actor_walk(s,a,minutes*v->walk_speed/1000.0f/s->config.minutes_per_tick,0,&barrier);
            (void)core_expansion_report_position(s->world,v->id,(int)(a->position.x*1000),(int)(a->position.y*1000));
        }
    }
}

static int target_valid(GameSession *s, const GameEnemy *e) {
    if (e->target_kind == TARGET_VILLAGER) {
        CoreVillagerView *v = game_villager(s, e->target_id);
        return v && exposed(s,v);
    }
    if (e->target_kind == TARGET_BUILDING) {
        CoreBuildingView *b = game_building(s, e->target_id);
        return b && b->hp > 0 && b->state == CORE_BSTATE_ACTIVE;
    }
    return 0;
}
static void choose_target(GameSession *s, GameEnemy *e) {
    float best = 8;
    int i;
    e->target_kind = TARGET_NONE;
    e->target_id = 0;
    for (i = 0; i < s->villager_count; i++) {
        CoreVillagerView *v = &s->villagers[i];
        GameActor *a = game_actor(s, v->id);
        float distance;
        if (!a || !exposed(s,v) || (!guard(v) && e->def.behavior != CORE_BEHAVIOR_MONSTER))
            continue;
        distance = game_distance(e->actor.position, a->position);
        if (distance <= best) {
            best = distance;
            e->target_kind = TARGET_VILLAGER;
            e->target_id = v->id;
        }
    }
    if (e->target_kind != TARGET_NONE)
        return;
    best = 1e9f;
    for (i = 0; i < s->building_count; i++) {
        CoreBuildingView *b = &s->buildings[i];
        float distance;
        if (b->hp <= 0 || b->state != CORE_BSTATE_ACTIVE || b->type == CORE_BLD_WALL ||
            b->type == CORE_BLD_GATE)
            continue;
        if (e->def.behavior == CORE_BEHAVIOR_THIEF && b->type != CORE_BLD_KEEP &&
            b->type != CORE_BLD_WAREHOUSE)
            continue;
        distance = game_distance(e->actor.position, center(b));
        if (e->def.behavior == CORE_BEHAVIOR_RAIDER)
            distance +=
                game_distance(center(b), (GamePoint){s->summary.keep_x + 0.5f, s->summary.keep_y + 0.5f}) *
                0.3f;
        if (e->def.behavior == CORE_BEHAVIOR_MONSTER && b->type == CORE_BLD_HOUSE)
            distance *= 0.7f;
        if (distance < best) {
            best = distance;
            e->target_kind = TARGET_BUILDING;
            e->target_id = b->id;
        }
    }
}
static GamePoint building_approach(const GameSession *s, const CoreBuildingView *b, GamePoint from) {
    GamePoint best = {b->door_x + 0.5f, b->door_y + 0.5f};
    float distance = 1e9f;
    int x, y, w = s->summary.map_width, h = s->summary.map_height;
    for (y = b->y - 1; y <= b->y + b->h; y++)
        for (x = b->x - 1; x <= b->x + b->w; x++) {
            GamePoint p = {x + 0.5f, y + 0.5f};
            float d;
            if (x < 0 || y < 0 || x >= w || y >= h ||
                (x >= b->x && y >= b->y && x < b->x + b->w && y < b->y + b->h))
                continue;
            if (!game_path_cost(s->cost[y * w + x], 1))
                continue;
            d = game_distance(from, p);
            if (d < distance) {
                best = p;
                distance = d;
            }
        }
    return best;
}
static int plan_building_side(GameSession *s,GameEnemy *e,const CoreBuildingView *b,int mode) {
    int width=s->summary.map_width,height=s->summary.map_height;
    for(int y=b->y-1;y<=b->y+b->h;y++)for(int x=b->x-1;x<=b->x+b->w;x++) {
        if(x<0||y<0||x>=width||y>=height||(x>=b->x&&x<b->x+b->w&&y>=b->y&&y<b->y+b->h))continue;
        if(!game_path_cost(s->cost[y*width+x],mode))continue;
        if(game_actor_plan(s,&e->actor,x,y,mode)>0)return 1;
    }
    return 0;
}
static int plan_attack_alternative(GameSession *s,GameEnemy *e,int mode) {
    /* 가장 가까운 면이 막혔다면 다른 면, 그 다음 실제로 접근 가능한 시설을 공략합니다. */
    int previous=e->target_kind==TARGET_BUILDING?e->target_id:0;
    CoreBuildingView *target=game_building(s,previous);
    if(target && building_distance(e->actor.position,target)<=fmaxf(1,(float)e->def.range)) {e->target_kind=TARGET_BUILDING;e->target_id=target->id;e->retarget=30;return 2;}
    if(target && plan_building_side(s,e,target,mode))return 1;
    float minimum=-1;int previous_id=0;
    for(int attempt=0;attempt<s->building_count;attempt++) {
        CoreBuildingView *candidate=NULL;float best=1e9f;
        for(int i=0;i<s->building_count;i++) {
            CoreBuildingView *b=&s->buildings[i];
            if(b->id==previous||b->hp<=0||b->state!=CORE_BSTATE_ACTIVE||(b->kind_flags&CORE_EXP_ROAD))continue;
            float score=game_distance(e->actor.position,center(b));
            if(score<minimum || (score==minimum && b->id<=previous_id))continue;
            if(!candidate || score<best || (score==best && b->id<candidate->id)){candidate=b;best=score;}
        }
        if(!candidate)break;
        minimum=best;previous_id=candidate->id;
        if(building_distance(e->actor.position,candidate)<=fmaxf(1,(float)e->def.range)) {
            e->target_kind=TARGET_BUILDING;e->target_id=candidate->id;e->retarget=30;return 2;
        }
        if(plan_building_side(s,e,candidate,mode)) {
            e->target_kind=TARGET_BUILDING;e->target_id=candidate->id;e->retarget=30;return 1;
        }
    }
    e->retarget=2;return 0;
}
static void hostile_area(GameSession *s,GamePoint p,float radius,int damage,int exclude_kind,int exclude_id,int source_id) {
    for(int i=0;i<s->villager_count;i++) {
        CoreVillagerView *v=&s->villagers[i];GameActor *a=game_actor(s,v->id);
        if(a && exposed(s,v) && !(exclude_kind==TARGET_VILLAGER&&exclude_id==v->id) && game_distance(a->position,p)<=radius) {
            int rc=s->summary.ruleset?core_expansion_person_hit(s->world,v->id,damage,1,source_id):core_report_villager_damage(s->world,s->raid_id,v->id,damage);
            if(rc==CORE_OK)game_hit_fx_direction(s,p,a->position,7);
        }
    }
    for(int i=0;i<s->building_count;i++) {
        CoreBuildingView *b=&s->buildings[i];
        if(b->state==CORE_BSTATE_ACTIVE && !(exclude_kind==TARGET_BUILDING&&exclude_id==b->id) && building_distance(p,b)<=radius) {
            int rc=s->summary.ruleset?core_expansion_building_hit(s->world,b->id,damage,1,source_id):core_report_building_damage(s->world,s->raid_id,b->id,damage);
            if(rc==CORE_OK)game_hit_fx_direction(s,p,center(b),7);
        }
    }
}
static void attack_impact(GameSession *s,GameEnemy *e) {
    int damage=e->def.damage,kind=e->attack_kind,id=e->attack_target;
    if(e->info.faction==0 && e->def.type!=3)
        for(int i=0;i<s->enemy_count;i++) if(s->enemies[i].hp>0 && !s->enemies[i].pending_spawn && s->enemies[i].def.type==3 && game_distance(e->actor.position,s->enemies[i].actor.position)<=5) {damage=(damage*110+50)/100;break;}
    CoreBuildingView *b=kind==TARGET_BUILDING?game_building(s,id):NULL;
    if(b && (b->kind_flags&(CORE_EXP_WALL|CORE_EXP_GATE))) {
        if(e->info.ability==2)damage*=2;
        if(e->info.ability==6)damage=damage*3/2;
    }
    if(e->def.range>1) {
        fire(s,e->actor.position,e->attack_to,kind,id,damage,e->info.ability==10?1.0f:0.0f,e->def.type,e->actor.id);
    } else {
        int rc;
        if(kind==TARGET_BUILDING)rc=s->summary.ruleset?core_expansion_building_hit(s->world,id,damage,1,e->actor.id):core_report_building_damage(s->world,s->raid_id,id,damage);
        else rc=s->summary.ruleset?core_expansion_person_hit(s->world,id,damage,1,e->actor.id):core_report_villager_damage(s->world,s->raid_id,id,damage);
        if(rc==CORE_OK)game_hit_fx_direction(s,e->actor.position,e->attack_to,e->info.ability==2 || e->info.ability==6 || e->info.ability==8?2:0);
        if(e->info.ability==8) hostile_area(s,e->attack_to,1.5f,damage/2,kind,id,e->actor.id);
    }
}
static void attack(GameSession *s,GameEnemy *e,int kind,int id,GamePoint to) {
    if(e->actor.cooldown>0)return;
    if(s->summary.ruleset)(void)core_expansion_engaged(s->world,e->actor.id);
    e->actor.cooldown+=(float)e->def.cooldown;
    e->attack_elapsed=0;e->attack_serial++;e->attack_applied=0;e->attack_kind=kind;e->attack_target=id;e->attack_to=to;
    e->actor.facing=game_walk_facing(to.x-e->actor.position.x,to.y-e->actor.position.y,e->actor.facing);
}
static void enemy_step(GameSession *s, GameEnemy *e, float minutes) {
    GamePoint goal = e->actor.position;
    float distance = 1e9f;
    int barrier = 0, arrived;
    if(e->pending_spawn) {
        if(s->raid.phase==CORE_RAID_RETREAT) {
            (void)core_report_enemy_escaped(s->world,s->raid_id,e->actor.id);e->hp=0;return;
        }
        e->retarget-=minutes;if(e->retarget>0)return;
        e->pending_spawn=0;e->retarget=0;
    }
    e->actor.cooldown=fmaxf(0,e->actor.cooldown)-minutes;
    if(s->summary.ruleset && s->raid.phase==CORE_RAID_RETREAT) {
        if(!e->retreat_started) {
            e->retreat_started=1;e->fleeing=1;e->retreat_index=e->inbound_count-1;
            e->retarget=0;
            e->attack_serial=0;e->target_kind=TARGET_NONE;e->actor.grid_revision=-1;
        }
    }
    /* 주기 능력은 공격 자세를 재생하는 시간에도 같은 게임 시계를 사용합니다. */
    e->special_clock += minutes;
    if(s->summary.ruleset && !e->fleeing && e->special_clock>=120) {
        if(e->info.ability==4) {int32_t id=0;if(core_expansion_summon(s->world,e->actor.id,&id)!=CORE_OK)e->special_clock=118;}
        if(e->info.ability==5) {
            for(int i=0;i<s->enemy_count;i++) {GameEnemy *other=&s->enemies[i];if(other!=e && other->hp>0 && !other->pending_spawn && other->hp<other->def.hp && game_distance(e->actor.position,other->actor.position)<=5){other->hp+=15;if(other->hp>other->def.hp)other->hp=other->def.hp;break;}}
        }
        if(e->special_clock>=120)e->special_clock-=120;
    }
    if(e->attack_serial && e->attack_elapsed < 4.8f) {
        e->attack_elapsed += minutes;
        if(!e->attack_applied && e->attack_elapsed>=2.4f) {e->attack_applied=1;attack_impact(s,e);}
        return;
    }
    e->retarget -= minutes;
    if (e->fleeing) {
        goal = (GamePoint){e->entry_x+.5f,e->entry_y+.5f};
        while(e->retreat_index>=0) {
            GamePoint waypoint={e->inbound[e->retreat_index*2]+.5f,e->inbound[e->retreat_index*2+1]+.5f};
            if(game_distance(e->actor.position,waypoint)<.7f){e->retreat_index--;continue;}
            goal=waypoint;break;
        }
    } else {
        if (!target_valid(s, e) || e->retarget <= 0) {
            int previous_id=e->target_id,previous_kind=e->target_kind;
            choose_target(s, e);
            if(previous_id!=e->target_id || previous_kind!=e->target_kind)e->actor.grid_revision=-1;
            e->retarget = 30;
        }
        if (e->target_kind == TARGET_VILLAGER) {
            GameActor *v = game_actor(s, e->target_id);
            if (!v)
                return;
            goal = v->position;
            distance = game_distance(e->actor.position, goal);
        } else if (e->target_kind == TARGET_BUILDING) {
            CoreBuildingView *b = game_building(s, e->target_id);
            if (!b)
                return;
            goal = e->actor.grid_revision==s->grid_revision && e->actor.next<e->actor.path.count
                ? (GamePoint){e->actor.target_x+.5f,e->actor.target_y+.5f}:building_approach(s,b,e->actor.position);
            distance = building_distance(e->actor.position, b);
            if (!e->looted && e->def.loot_amount > 0 && e->def.behavior != CORE_BEHAVIOR_RAIDER &&
                (b->type == CORE_BLD_KEEP || b->type == CORE_BLD_WAREHOUSE) && distance <= 1.0f) {
                int32_t amount = 0;
                if (core_report_loot(s->world, s->raid_id, e->actor.id, b->id, &amount) == CORE_OK) {
                    e->looted = 1;
                    e->fleeing = 1;
                    e->actor.grid_revision = -1;
                    s->loots++;
                    return;
                }
            }
        } else
            return;
        if (distance <= fmaxf(1, (float)e->def.range)) {
            CoreBuildingView *b=e->target_kind==TARGET_BUILDING?game_building(s,e->target_id):NULL;
            attack(s, e, e->target_kind, e->target_id, b?center(b):goal);
            return;
        }
    }
    if (e->actor.grid_revision != s->grid_revision || e->actor.target_x != (int)goal.x ||
        e->actor.target_y != (int)goal.y || !e->actor.path.count || e->actor.next>=e->actor.path.count) {
        if(e->fleeing && e->retarget>0)return;
        int mode=e->info.ability==3?2:e->fleeing?3:1;
        if(game_actor_plan(s,&e->actor,(int)goal.x,(int)goal.y,mode)<=0) {
            if(!e->fleeing){
                int alternative=plan_attack_alternative(s,e,mode);
                if(!alternative)return;
                if(alternative==2){CoreBuildingView *b=game_building(s,e->target_id);if(b)attack(s,e,TARGET_BUILDING,b->id,center(b));return;}
            }
            else {
            /* 막힌 옛 경유점을 건너뛰되 위치를 바꾸지 않고 실제 우회로를 찾습니다. */
            int planned=0;
            for(int retry=0;retry<8 && e->retreat_index>=0;retry++) {
                e->retreat_index--;
                int x=e->retreat_index>=0?e->inbound[e->retreat_index*2]:e->entry_x;
                int y=e->retreat_index>=0?e->inbound[e->retreat_index*2+1]:e->entry_y;
                if(game_actor_plan(s,&e->actor,x,y,mode)>0){planned=1;break;}
            }
            if(!planned && game_actor_plan(s,&e->actor,-1,-1,mode)>0) {
                e->retreat_index=-1;e->entry_x=e->actor.target_x;e->entry_y=e->actor.target_y;planned=1;
            }
            if(!planned){e->retarget=10;return;}
            }
        }
    }
    GamePoint before=e->actor.position;
    arrived = game_actor_walk(s, &e->actor, minutes * e->def.speed / 1000.0f / s->config.minutes_per_tick, e->info.ability==3?2:e->fleeing?3:1,
                              &barrier);
    if(game_distance(before,e->actor.position)<.001f && !barrier && !e->fleeing) {
        e->stalled+=minutes;
        if(e->stalled>=60) {
            int alternative=plan_attack_alternative(s,e,e->info.ability==3?2:1);
            if(alternative==2){CoreBuildingView *b=game_building(s,e->target_id);if(b)attack(s,e,TARGET_BUILDING,b->id,center(b));}
            e->stalled=0;e->actor.grid_revision=-1;
        }
    } else e->stalled=0;
    if (barrier) {
        if(e->fleeing){e->actor.grid_revision=-1;return;}
        CoreBuildingView *b = game_building(s, barrier);
        if (b && b->hp > 0 && e->actor.cooldown <= 0) {
            attack(s, e, TARGET_BUILDING, barrier, center(b));
            s->barrier_hits++;
        }
    }
    if (arrived && e->fleeing && e->retreat_index<0 &&
        ((int)e->actor.position.x==0 || (int)e->actor.position.y==0 || (int)e->actor.position.x==s->summary.map_width-1 || (int)e->actor.position.y==s->summary.map_height-1)) {
        (void)core_report_enemy_escaped(s->world, s->raid_id, e->actor.id);
        e->hp = 0;
    }
}
void game_combat_step(GameSession *s, float minutes) {
    int i;
    if (s->summary.ruleset && s->summary.raid_id && s->alarm_seen_id != s->summary.raid_id) {
        s->alarm_seen_id = s->summary.raid_id;
        s->dispatch_delay = researched(s, 15) ? 16.0f : 20.0f;
    } else if (s->dispatch_delay > 0)
        s->dispatch_delay -= minutes;
    projectiles_step(s, minutes);
    if (!s->raid_id) {
        towers_step(s, minutes);
        guards_step(s, minutes);
        return;
    }
    for (i = 0; i < s->enemy_count; i++)
        if (s->enemies[i].hp > 0)enemy_step(s, &s->enemies[i], minutes);
    /* 가까운 적끼리는 통행 가능한 칸 안에서 조금씩 떨어집니다. 경로를 막지는 않습니다. */
    for(i=0;i<s->enemy_count;i++) {
        GameEnemy *e=&s->enemies[i];if(e->hp<=0||e->pending_spawn)continue;
        GamePoint delta={0,0};
        for(int j=0;j<s->enemy_count;j++) {
            GameEnemy *other=&s->enemies[j];if(j==i||other->hp<=0||other->pending_spawn)continue;
            float dx=e->actor.position.x-other->actor.position.x,dy=e->actor.position.y-other->actor.position.y;
            float d=sqrtf(dx*dx+dy*dy);if(d>=.55f)continue;
            if(d<.001f){float angle=(e->actor.id*2.399963f);dx=cosf(angle);dy=sinf(angle);d=1;}
            delta.x+=dx/d;delta.y+=dy/d;
        }
        float length=sqrtf(delta.x*delta.x+delta.y*delta.y);
        if(length>.001f) {
            float step=fminf(.06f,minutes*.025f);
            GamePoint p={e->actor.position.x+delta.x/length*step,e->actor.position.y+delta.y/length*step};
            int x=(int)floorf(p.x),y=(int)floorf(p.y);
            if(x>=0&&y>=0&&x<s->summary.map_width&&y<s->summary.map_height && s->cost[y*s->summary.map_width+x]>0 && s->cost[y*s->summary.map_width+x]!=CORE_COST_WALL && s->cost[y*s->summary.map_width+x]!=CORE_COST_GATE)e->actor.position=p;
        }
    }
    if(s->summary.ruleset && s->raid.phase==CORE_RAID_RETREAT && s->visibility) {
        int raid_id=s->raid_id;
        for(i=0;i<s->enemy_count;i++)if(s->enemies[i].hp>0) {
            GameEnemy *e=&s->enemies[i];int x=(int)e->actor.position.x,y=(int)e->actor.position.y;
            if(s->visibility[y*s->summary.map_width+x]<2) {
                (void)core_report_enemy_escaped(s->world,raid_id,e->actor.id);e->hp=0;
            }
        }
        if(game_combat_sync(s)!=CORE_OK)return;
    }
    if (s->summary.ruleset && s->visibility)
        for (i = 0; i < s->enemy_count; i++)
            if (s->enemies[i].hp > 0 && !s->enemies[i].pending_spawn) {
                int x = (int)s->enemies[i].actor.position.x, y = (int)s->enemies[i].actor.position.y;
                if (x >= 0 && y >= 0 && x < s->summary.map_width && y < s->summary.map_height &&
                    s->visibility[y * s->summary.map_width + x] == 2)
                    (void)core_expansion_spotted(s->world, x, y);
            }
    towers_step(s, minutes);
    guards_step(s, minutes);
    if(s->summary.ruleset && s->raid_id)for(int n=0;n<s->enemy_count;n++) {
        GameEnemy *e=&s->enemies[n];if(e->hp<=0)continue;
        int x=(int)e->actor.position.x,y=(int)e->actor.position.y;
        if(!e->pending_spawn && !e->fleeing && (!e->inbound_count || abs(e->inbound[(e->inbound_count-1)*2]-x)+abs(e->inbound[(e->inbound_count-1)*2+1]-y)>=3)) {
            if(e->inbound_count==64){memmove(e->inbound+2,e->inbound+4,124*sizeof(int));e->inbound_count=63;}
            e->inbound[e->inbound_count*2]=x;e->inbound[e->inbound_count*2+1]=y;e->inbound_count++;
        }
        CoreEnemyRuntime r={0};r.valid=e->pending_spawn?2:1;r.hp=e->hp;r.x_milli=(int)(e->actor.position.x*1000);r.y_milli=(int)(e->actor.position.y*1000);
        r.retarget_milli=(int)(fmaxf(0,e->retarget)*1000);r.entry_x=e->entry_x;r.entry_y=e->entry_y;r.cooldown_milli=(int)(fmaxf(0,e->actor.cooldown)*1000);
        r.special_milli=(int)(e->special_clock*1000);r.attack_elapsed_milli=(int)(e->attack_elapsed*1000);
        r.attack_serial=e->attack_serial;r.attack_applied=e->attack_applied;r.attack_kind=e->attack_kind;r.attack_target=e->attack_target;
        r.attack_x_milli=(int)(e->attack_to.x*1000);r.attack_y_milli=(int)(e->attack_to.y*1000);r.fleeing=e->fleeing;
        r.retreat_index=e->retreat_index;r.inbound_count=e->inbound_count;memcpy(r.inbound,e->inbound,sizeof(r.inbound));
        (void)core_expansion_enemy_runtime(s->world,e->actor.id,&r);
    }
    (void)game_combat_sync(s);
}
