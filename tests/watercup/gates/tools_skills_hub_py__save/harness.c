/* gate: tools_skills_hub_py__save — traced copy of sh_save */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sh_save(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("sh_save");
    int result = sh_save(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sh_save");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sh_save <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
