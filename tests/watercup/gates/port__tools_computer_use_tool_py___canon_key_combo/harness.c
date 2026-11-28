/* gate: port__tools_computer_use_tool_py___canon_key_combo -> real canon_key_combo (sorted JSON array). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern char *tools_computer_use_tool_u_canon_key_combo(const char *keys);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <keys>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tools_computer_use_tool_u_canon_key_combo");
        fflush(stdout);
        char *r = tools_computer_use_tool_u_canon_key_combo(argv[i]);
        WCUP_VALSTR(r ? r : "[]");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("tools_computer_use_tool_u_canon_key_combo");
        free(r);
    }
    return 0;
}
