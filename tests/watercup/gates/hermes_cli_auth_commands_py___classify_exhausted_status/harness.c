/* gate: hermes_cli_auth_commands_py___classify_exhausted_status -> real 3-arg classification port. Input "code|||reason|||message". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *hermes_cli_auth_commands_u_classify_exhausted_status(const char *code, const char *reason, const char *message);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <code|||reason|||message>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[4096]; snprintf(buf, sizeof buf, "%s", argv[i]);
        char *p1 = strstr(buf, "|||");
        char *code = buf, *reason = "", *message = "";
        if (p1) { *p1 = 0; reason = p1 + 3; char *p2 = strstr(reason, "|||"); if (p2) { *p2 = 0; message = p2 + 3; } }
        WCUP_BEGIN("hermes_cli_auth_commands_u_classify_exhausted_status");
        fflush(stdout);
        char *r = hermes_cli_auth_commands_u_classify_exhausted_status(code, reason, message);
        WCUP_VALSTR(r ? r : "");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("hermes_cli_auth_commands_u_classify_exhausted_status");
        free(r);
    }
    return 0;
}
