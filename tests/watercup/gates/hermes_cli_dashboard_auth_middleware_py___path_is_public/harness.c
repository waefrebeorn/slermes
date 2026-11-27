/* gate: dashboard auth allowlist — which paths skip authentication?
 *
 * Python: PUBLIC_API_PATHS and the icon set matched exactly, _GATE_PUBLIC_PREFIXES
 * prefix-matched. The exactness is the security property: /api/status is public,
 * /api/status/extension must not be.
 *
 * The port used to take an invented "path\tpublic_paths\tpublic_prefixes" argument
 * and print its answer, so the real allowlists were never consulted and no caller
 * could obtain the decision.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_dashboard_auth_midd_u_path_is_public(const char *path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_path_public <path>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[8];
        WCUP_BEGIN("wc_path_public");
        WCUP_BRANCH(argv[i][0] == '/');                  /* B1: a real path */
        snprintf(buf, sizeof buf, "%d", hermes_cli_dashboard_auth_midd_u_path_is_public(argv[i]));
        WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("wc_path_public");
    }
    return 0;
}
