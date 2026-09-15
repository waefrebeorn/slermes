/* gate: hermes_cli_update_cmd_py___editable_install_is_current — traced copy of upd_editable_install_is_current */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int upd_editable_install_is_current(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("upd_editable_install_is_current");
    int result = upd_editable_install_is_current(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("upd_editable_install_is_current");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s upd_editable_install_is_current <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
