/* 원본 비교용 시트와 픽셀 조명 자료를 만드는 창 없는 도구입니다. */
#include "lighting_pixels.h"
#include "storage.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "--check"))
        return art_pixel_check(argv[2]);
    if (argc == 4 && !strcmp(argv[1], "--reference"))
        return art_pixel_reference(argv[2], argv[3]);
    if (argc == 4 && !strcmp(argv[1], "--scene"))
        return art_pixel_scene(argv[2], argv[3]);
    if (argc == 4 && !strcmp(argv[1], "--bake")) {
        if (!game_make_dirs(argv[3]))
            return 1;
        return art_pixel_lighting(argv[2], argv[3]);
    }
    fprintf(stderr, "Usage: castle_pixel_lighting --reference ROOT OUTPUT | --bake SOURCES OUTPUT | --scene "
                    "ROOT OUTPUT | --check ROOT\n");
    return 2;
}
