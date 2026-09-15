/* gate: port__hermes_cli_update_inventory_py__collect_runtime_inventory — traced copy of collect_runtime_inventory (int) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern int collect_runtime_inventory(upd_plan_t *out_plan);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s collect_runtime_inventory <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("collect_runtime_inventory");
        int result = collect_runtime_inventory(argv[i]);
            char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("collect_runtime_inventory");
    }
    return 0;
}
