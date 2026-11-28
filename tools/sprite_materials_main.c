/* GPU 없이 재료를 제작·검사하는 개발 도구입니다. */
#include "art/sprite_materials.h"
#include "raylib.h"
#include "storage.h"
#include <string.h>
int main(int argc, char **argv) {
    GameArguments args;
    if (!game_arguments(&args, argc, argv))
        return 2;
    SetTraceLogLevel(LOG_WARNING);
    int result = 2;
    if (args.count == 2 && !strcmp(args.values[1], "--test"))
        result = art_sprite_materials_test();
    else if (args.count == 5 && !strcmp(args.values[1], "--bake"))
        result = art_sprite_materials(args.values[2], args.values[3], args.values[4], 0);
    else if (args.count == 4 && !strcmp(args.values[1], "--check"))
        result = art_sprite_materials_check(args.values[2], args.values[3]);
    else if (args.count == 4 && !strcmp(args.values[1], "--gallery"))
        result = art_sprite_materials_gallery(args.values[2], args.values[3]);
    game_arguments_free(&args);
    return result;
}
