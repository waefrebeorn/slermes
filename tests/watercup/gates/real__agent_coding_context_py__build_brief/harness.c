/* gate: the coding posture's prefix brief. Fixture form: "<model>|<-1|0|1>".
 * Compared as hex: the brief is multi-kilobyte with newlines and backticks. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
char *coding_context_build_brief(const char *model, int has_todo);
static void to_hex(const char *s, char *out, size_t cap) {
    size_t p = 0;
    for (const unsigned char *c = (const unsigned char *)(s ? s : ""); *c && p + 3 < cap; c++)
        p += (size_t)snprintf(out + p, cap - p, "%02x", *c);
    out[p] = '\0';
}
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    static char hex[65536];
    for (int i = 2; i < argc; i++) {
        char work[512]; snprintf(work, sizeof work, "%s", argv[i]);
        char *bar = strchr(work, '|');
        int has_todo = -1; const char *model = work;
        if (bar) { *bar = 0; has_todo = atoi(bar + 1); }
        WCUP_BEGIN("gate");
        char *brief = coding_context_build_brief(strcmp(model, "None") == 0 ? NULL : model, has_todo);
        to_hex(brief, hex, sizeof hex);
        WCUP_VALSTR(hex);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("gate");
        if (brief) free(brief);
    }
    return 0;
}
