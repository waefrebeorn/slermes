#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
/* gate: traced copy of src/tools/tts_tool_ports.c::tts_resolve_max_text_length
 * (PoP: tools/tts_tool.py:_resolve_max_text_length). Input encoding:
 *   provider\x1foverride   override: '-' absent | 'b' bool | integer text
 * C port resolves ONLY user override -> FALLBACK(4000); LIVE adds elevenlabs
 * model table / provider table / command-provider rungs, so providers outside
 * those tables are used (see notes). */
static long resolve_one(const char *prov, const char *ov) {
    long res = 4000;
    WCUP_BEGIN("resolve_max_text_length");
    if (!prov || !*prov) { WCUP_BRANCH(1); res = 4000; }
    else {
        WCUP_BRANCH(0);
        int have_ov = (ov && strcmp(ov, "-") != 0);
        if (have_ov) {
            WCUP_BRANCH(1);
            if (strcmp(ov, "b") == 0) { WCUP_BRANCH(1); /* bool override never wins */ }
            else {
                WCUP_BRANCH(0);
                long v = atol(ov);
                if (v > 0) { WCUP_BRANCH(1); res = v; }
                else WCUP_BRANCH(0);
            }
        } else WCUP_BRANCH(0);
    }
    char vbuf[32]; snprintf(vbuf, sizeof vbuf, "%ld", res);
    WCUP_VALSTR(vbuf);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("resolve_max_text_length");
    return res;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve_max_text_length <prov\\x1fov>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *s = strdup(argv[i]);
        char *sep = strchr(s, '\x1f');
        const char *ov = "-";
        if (sep) { *sep = '\0'; ov = sep + 1; }
        resolve_one(s, ov);
        free(s);
    }
    return 0;
}
