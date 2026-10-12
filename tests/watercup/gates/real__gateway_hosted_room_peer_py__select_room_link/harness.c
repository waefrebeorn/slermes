/* gate: real__gateway_hosted_room_peer_py__select_room_link — GAP sentinel for select_room_link */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "select_room_link";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s select_room_link <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("select_room_link");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("select_room_link");
    }
    return 0;
}
