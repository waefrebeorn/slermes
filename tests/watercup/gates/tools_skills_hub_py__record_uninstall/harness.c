/* gate: tools_skills_hub_py__record_uninstall — traced copy of hub_lock_record_uninstall */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool hub_lock_record_uninstall(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hub_lock_record_uninstall");
    bool result = hub_lock_record_uninstall(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hub_lock_record_uninstall");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hub_lock_record_uninstall <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
