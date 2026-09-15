/* gate: agent_pet_render_py__kitty_image_id — traced copy of pet_kitty_image_id */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pet_kitty_image_id(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pet_kitty_image_id");
    int result = pet_kitty_image_id(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pet_kitty_image_id");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pet_kitty_image_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
