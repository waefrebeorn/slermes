/* gate: tools_skill_manager_tool_py___apply_skill_write_gate — traced copy of smt_u_apply_skill_write_gate */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int smt_u_apply_skill_write_gate(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("smt_u_apply_skill_write_gate");
    int result = smt_u_apply_skill_write_gate(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("smt_u_apply_skill_write_gate");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s smt_u_apply_skill_write_gate <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
