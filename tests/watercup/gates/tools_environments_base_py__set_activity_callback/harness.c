/* gate: tools_environments_base_py__set_activity_callback — traced copy of envb_set_activity_callback */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envb_set_activity_callback(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envb_set_activity_callback");
    int result = envb_set_activity_callback(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envb_set_activity_callback");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envb_set_activity_callback <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
