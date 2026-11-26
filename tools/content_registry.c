/* content_registry.c — 공개 API에서 현재 콘텐츠 원장을 직접 생성합니다. */
#include <castle_core.h>
#include <castle_expansion.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void quoted(const char *s) {
    putchar('"');
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\')
            putchar('\\');
        if (c < 32)
            printf("\\u%04x", c);
        else
            putchar(c);
    }
    putchar('"');
}
static void array(const int32_t *v, int n) {
    putchar('[');
    for (int i = 0; i < n; i++)
        printf("%s%d", i ? "," : "", v[i]);
    putchar(']');
}
int main(int argc, char **argv) {
    CoreNewGameParams params = {0};
    CoreWorld *world = NULL;
    params.seed = argc > 1 ? atoi(argv[1]) : 42;
    params.ruleset = 1;
    strcpy(params.castle_name, "Registry");
    if (core_create(&params, NULL, &world) != CORE_OK)
        return 1;
    printf("{\n\"buildings\":[");
    for (int i = 0; i < CORE_BLD_COUNT; i++) {
        CoreExpansionBuildingInfo b;
        if (core_expansion_building_info(i, &b) != CORE_OK) {
            core_destroy(world);
            return 1;
        }
        printf("%s{\"id\":%d,\"key\":", i ? "," : "", i);
        quoted(b.key);
        printf(",\"name\":");
        quoted(b.name);
        printf(",\"width\":%d,\"height\":%d,\"rank\":%d,\"workers\":%d,\"category\":%d,\"cost\":", b.w, b.h,
               b.rank, b.workers, b.category);
        array(b.cost, CORE_RES_COUNT);
        printf("}");
    }
    printf("],\n\"animals\":[");
    for (int i = 0; i < CORE_EXP_ANIMALS; i++) {
        CoreAnimalInfo a;
        if (core_expansion_animal_info(i, &a) != CORE_OK) {
            core_destroy(world);
            return 1;
        }
        printf("%s{\"id\":%d,\"name\":", i ? "," : "", i);
        quoted(a.name);
        printf(",\"hp\":%d,\"attack\":%d,\"food\":%d,\"leather\":%d,\"biomass\":%d,\"speed\":%d,\"swim\":%d,"
               "\"livestock\":%d}",
               a.hp, a.attack, a.food, a.leather, a.biomass, a.speed_milli, a.swim_milli, a.livestock);
    }
    printf("],\n\"resources\":[");
    for (int i = 0; i < CORE_RES_COUNT; i++) {
        printf("%s{\"id\":%d,\"key\":", i ? "," : "", i);
        quoted(core_expansion_resource_key(i));
        printf("}");
    }
    printf("],\n\"research\":[");
    for (int i = 0; i < 128; i++) {
        CoreResearchView r;
        if (core_get_research(world, i, &r) != CORE_OK)
            break;
        printf("%s{\"id\":%d,\"key\":", i ? "," : "", i);
        quoted(r.key);
        printf(",\"rank\":%d,\"gold\":%d,\"mana\":%d,\"points\":%d}", r.required_rank, r.cost_gold,
               r.cost_mana, r.points);
    }
    printf("],\n\"barter\":[");
    for (int i = 1; i <= core_expansion_barter_count(); i++) {
        CoreBarterRecipe r;
        if (core_expansion_barter_recipe(i, &r) != CORE_OK) {
            core_destroy(world);
            return 1;
        }
        printf("%s{\"id\":%d,\"rank\":%d,\"pay\":%d,\"pay_count\":%d,\"get\":%d,\"get_count\":%d,\"max_"
               "uses\":%d,\"weight\":%d,\"benefit_bp\":%d}",
               i > 1 ? "," : "", r.id, r.rank, r.pay_resource, r.pay_count, r.get_resource, r.get_count,
               r.max_uses, r.weight, r.benefit_bp);
    }
    CoreAnimalView *animals = malloc(sizeof(*animals) * CORE_EXP_MAX_ANIMALS);
    int count = 0, wild = 0;
    if (!animals || core_expansion_copy_animals(world, animals, CORE_EXP_MAX_ANIMALS, &count) != CORE_OK) {
        free(animals);
        core_destroy(world);
        return 1;
    }
    for (int i = 0; i < count; i++)
        wild += animals[i].alive && !animals[i].owner;
    free(animals);
    printf("],\n\"initial_world\":{\"seed\":%d,\"wild\":%d}}\n", params.seed, wild);
    core_destroy(world);
    return 0;
}
