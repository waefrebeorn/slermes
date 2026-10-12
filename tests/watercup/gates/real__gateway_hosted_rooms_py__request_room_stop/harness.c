/* gate: real__gateway_hosted_rooms_py__request_room_stop — GAP sentinel for request_room_stop */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "request_room_stop";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s request_room_stop <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("request_room_stop");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("request_room_stop");
    }
    return 0;
}
