/* gate: get_inbound_media_max_bytes — CONVERTED from a GAP sentinel to the real port symbol get_inbound_media_max_bytes.
 * The sentinel returned the function name for every input; this calls get_inbound_media_max_bytes().
 * Kept only because its cups equalled the LIVE Python twin's with this shape. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

static void trace_one(const char *input) {
    WCUP_BEGIN("get_inbound_media_max_bytes");
extern long get_inbound_media_max_bytes(const char *input);
    long r = get_inbound_media_max_bytes(input);
    WCUP_VALINT(r);
    char cup[64];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("get_inbound_media_max_bytes");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *input = strcmp(argv[i], "NULL") == 0 ? NULL : argv[i];
        trace_one(input);
    }
    return 0;
}
