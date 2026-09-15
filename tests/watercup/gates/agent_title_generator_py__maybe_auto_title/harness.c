/* gate: agent_title_generator_py__maybe_auto_title — traced copy of tig_maybe_auto_title */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tig_maybe_auto_title(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("tig_maybe_auto_title");
    int result = tig_maybe_auto_title(a, b);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tig_maybe_auto_title");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tig_maybe_auto_title <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
