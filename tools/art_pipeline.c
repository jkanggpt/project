/* art_pipeline.c — 원본에서 현재 런타임 에셋만 발행하는 단일 진입점 */
#include "art/image_ops.h"
#include "art/pipeline.h"
#include "art/sprite_materials.h"
#include "art/carried_lights.h"
#include "assets_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CASTLE_SOURCE_DIR
#define CASTLE_SOURCE_DIR "."
#endif
static char sources[GAME_PATH_BYTES], work[GAME_PATH_BYTES], painted[GAME_PATH_BYTES],
    settlement[GAME_PATH_BYTES];
static char paths[128][GAME_PATH_BYTES], keys[128][256];
static int path_count;
const char *art_input(const char *relative) {
    for (int i = 0; i < path_count; i++)
        if (!strcmp(keys[i], relative))
            return paths[i];
    if (path_count >= 128 || strlen(relative) >= sizeof(keys[0]))
        return "";
    int i = path_count++;
    strcpy(keys[i], relative);
    if (!game_join_path(paths[i], sizeof(paths[i]), sources, relative))
        return "";
    return paths[i];
}
const char *art_stage_output(void) { return settlement; }
const char *art_painted_output(void) { return painted; }
const char *art_stage_file(const char *relative) {
    static char path[GAME_PATH_BYTES];
    if (!game_join_path(path, sizeof(path), settlement, relative))
        return "";
    return path;
}
static int stage(int (*run)(int, char **), const char *input, const char *output) {
    char *args[3] = {"art-stage", (char *)input, (char *)output};
    int result = run(3, args);
    art_image_reset();
    if (!result) {
        args[1] = "--check";
        result = run(3, args);
        art_image_reset();
    }
    return result;
}
static int copy_file(const char *source, const char *destination) {
    int size = 0;
    unsigned char *bytes = game_read_bytes(source, &size);
    if (!bytes) {
        fprintf(stderr, "Missing art input: %s\n", source);
        return 0;
    }
    char parent[GAME_PATH_BYTES];
    snprintf(parent, sizeof(parent), "%s", destination);
    char *slash = strrchr(parent, '/'), *back = strrchr(parent, '\\');
    if (back && (!slash || back > slash))
        slash = back;
    if (slash) {
        *slash = 0;
        if (!game_make_dirs(parent)) {
            free(bytes);
            return 0;
        }
    }
    int ok = game_write_bytes(destination, bytes, (size_t)size);
    free(bytes);
    return ok;
}
int main(int argc, char **argv) {
    GameArguments args;
    if (!game_arguments(&args, argc, argv))
        return 2;
    const char *output = "game/assets";
    snprintf(sources, sizeof(sources), "%s/art/sources", CASTLE_SOURCE_DIR);
    snprintf(work, sizeof(work), "build/art-work");
    for (int i = 1; i < args.count; i++) {
        if (i + 1 >= args.count) {
            game_arguments_free(&args);
            return 2;
        }
        const char *option = args.values[i++], *value = args.values[i];
        if (!strcmp(option, "--sources"))
            snprintf(sources, sizeof(sources), "%s", value);
        else if (!strcmp(option, "--output"))
            output = value;
        else if (!strcmp(option, "--work"))
            snprintf(work, sizeof(work), "%s", value);
        else {
            game_arguments_free(&args);
            return 2;
        }
    }
    if (!game_join_path(painted, sizeof(painted), work, "painted") ||
        !game_join_path(settlement, sizeof(settlement), work, "settlement") || !game_make_dirs(painted) ||
        !game_make_dirs(settlement)) {
        game_arguments_free(&args);
        return 1;
    }
    SetTraceLogLevel(LOG_WARNING);
    int failed = stage(art_stage_base, art_input("base"), painted) ||
                 stage(art_stage_buildings, art_input("settlement"), settlement) ||
                 stage(art_stage_nature, art_input("nature"), settlement) ||
                 stage(art_stage_scenery, art_input("scenery"), settlement) ||
                 stage(art_stage_residents, art_input("residents"), settlement) ||
                 stage(art_stage_interface, "--effects", settlement) ||
                 stage(art_stage_interface, "--manor", settlement) ||
                 stage(art_stage_services, art_input("services"), settlement) ||
                 stage(art_stage_production, art_input("production"), settlement) ||
                 art_carried_lights(art_input("carried_lights/masks.txt"), settlement) ||
                 art_sprite_materials(sources, work, work, 1);
    for (int i = 0; !failed && i < CASTLE_ASSET_COUNT; i++) {
        const CastleAssetRecord *record = &CASTLE_ASSETS[i];
        char from[GAME_PATH_BYTES], to[GAME_PATH_BYTES];
        if (!strcmp(record->kind, "texture")) {
            if (!game_join_path(from, sizeof(from), work, record->stage_file)) {
                failed = 1;
                break;
            }
        } else {
            const char *source = record->source;
            const char *prefix = "art/sources/";
            if (strncmp(source, prefix, strlen(prefix))) {
                fprintf(stderr, "Invalid input recipe: %s\n", source);
                failed = 1;
                break;
            }
            snprintf(from, sizeof(from), "%s", art_input(source + strlen(prefix)));
        }
        if (!game_join_path(to, sizeof(to), output, record->path) || !copy_file(from, to))
            failed = 1;
    }
    if (!failed) {
        char from[GAME_PATH_BYTES], to[GAME_PATH_BYTES];
        snprintf(from, sizeof(from), "%s/game/assets/assets_manifest.json", CASTLE_SOURCE_DIR);
        if (!game_join_path(to, sizeof(to), output, "assets_manifest.json") || !copy_file(from, to))
            failed = 1;
    }
    printf("ART PIPELINE: assets=%d failed=%d\n", CASTLE_ASSET_COUNT, failed);
    game_arguments_free(&args);
    return failed ? 1 : 0;
}
