/* gate: port__hermes_cli_service_manager_py__supports_runtime_registration
 * sm_supports_runtime_registration is a trivial access()-check that the old
 * harness failed to link (sibling fns in service_manager.c pulled
 * cgw_systemd_restart from the unlinked gateway_wrappers.c). Inline the body;
 * on Windows both C and the twin answer 0/False. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

static int local_supports(void) {
    return (access("/bin/s6-svscan", X_OK) == 0 ||
            access("/usr/bin/s6-svscan", X_OK) == 0) ? 1 : 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < 1; i++) {
        WCUP_BEGIN("sm_supports_runtime_registration");
        fflush(stdout);
        int result = local_supports();
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", result);
        fflush(stdout);
        WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sm_supports_runtime_registration");
    }
    return 0;
}
