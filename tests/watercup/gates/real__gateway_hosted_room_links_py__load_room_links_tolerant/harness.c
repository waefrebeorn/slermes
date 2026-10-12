/* gate: real__gateway_hosted_room_links_py__load_room_links_tolerant — GAP sentinel for load_room_links_tolerant */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "load_room_links_tolerant";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s load_room_links_tolerant <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("load_room_links_tolerant");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("load_room_links_tolerant");
    }
    return 0;
}
