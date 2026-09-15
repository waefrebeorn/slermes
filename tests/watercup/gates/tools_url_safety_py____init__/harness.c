/* gate: tools_url_safety_py____init__ — traced copy of tools_url_safety_async_init */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_url_safety_async_init(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_url_safety_async_init");
    int result = tools_url_safety_async_init(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_url_safety_async_init");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_url_safety_async_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
