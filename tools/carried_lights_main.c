/* GPU 없이 랜턴·횃불 아틀라스를 재생성하고 프레임 경계를 검사합니다. */
#include "art/carried_lights.h"
#include "raylib.h"
#include "storage.h"
#include <string.h>
int main(int argc, char **argv) {
    GameArguments args;
    if (!game_arguments(&args, argc, argv))
        return 2;
    SetTraceLogLevel(LOG_WARNING);
    int result = 2;
    if (args.count == 3 && !strcmp(args.values[1], "--check"))
        result = art_carried_lights_check(args.values[2]);
    else if (args.count == 3) {
        result = art_carried_lights(args.values[1], args.values[2]);
        if (!result)
            result = art_carried_lights_check(args.values[2]);
    }
    game_arguments_free(&args);
    return result;
}
