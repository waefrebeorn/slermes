/* gate: gateway_platforms_weixin_py__restore — traced copy of wx2_restore */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wx2_restore(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wx2_restore");
    int result = wx2_restore(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wx2_restore");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wx2_restore <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
