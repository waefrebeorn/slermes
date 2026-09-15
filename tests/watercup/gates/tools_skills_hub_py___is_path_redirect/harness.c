/* gate: tools_skills_hub_py___is_path_redirect — traced copy of hub_is_path_redirect */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool hub_is_path_redirect(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hub_is_path_redirect");
    bool result = hub_is_path_redirect(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hub_is_path_redirect");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hub_is_path_redirect <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
