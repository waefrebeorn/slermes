/* gate: agent_auxiliary_client_py___is_codex_gpt54_or_gpt55 — traced copy of aux__is_codex_gpt54_or_gpt55 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aux__is_codex_gpt54_or_gpt55(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("aux__is_codex_gpt54_or_gpt55");
    int result = aux__is_codex_gpt54_or_gpt55(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aux__is_codex_gpt54_or_gpt55");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aux__is_codex_gpt54_or_gpt55 <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
