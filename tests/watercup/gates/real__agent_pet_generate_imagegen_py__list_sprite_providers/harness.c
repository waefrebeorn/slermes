/* gate: real__agent_pet_generate_imagegen_py__list_sprite_providers — GAP sentinel for list_sprite_providers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "list_sprite_providers";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s list_sprite_providers <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("list_sprite_providers");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("list_sprite_providers");
    }
    return 0;
}
