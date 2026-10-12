/* gate: real__gateway_hosted_room_peer_py__decode_room_grant — GAP sentinel for decode_room_grant */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "decode_room_grant";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s decode_room_grant <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("decode_room_grant");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("decode_room_grant");
    }
    return 0;
}
