/* GPU 없이 휴대 등불·고정 불 아틀라스를 재생성하고 칸 규약을 검사합니다.
   사용: castle_carried_art <art/sources> <출력 폴더> | --check <출력 폴더> */
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
        char carried[GAME_PATH_BYTES], stationary[GAME_PATH_BYTES];
        if (game_join_path(carried, sizeof(carried), args.values[1],
                           "graphics_finish_generated/carried_lights_padded.png") &&
            game_join_path(stationary, sizeof(stationary), args.values[1],
                           "graphics_finish_generated/stationary_fires_padded.png")) {
            result = art_carried_lights(carried, stationary, args.values[2]);
            if (!result)
                result = art_carried_lights_check(args.values[2]);
        }
    }
    game_arguments_free(&args);
    return result;
}
