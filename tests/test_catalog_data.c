#include "fixtures/content.h"

int test_content_parameter(const char *name) {
    int id = -1;
    if (sscanf(name, "content_recipe_%d", &id) == 1 && id >= 1 && id <= 85) {
        fx_content_recipe124(id);
        return 1;
    }
    if (sscanf(name, "content_animal_%d", &id) == 1 && id >= 0 && id < 35) {
        fx_content_animal124(id);
        return 1;
    }
    return 0;
}
