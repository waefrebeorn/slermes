/* gate: agent_system_prompt_py___tui_embedded_pane_clarifier
 * Calls the REAL port agent_system_prompt_u_tui_embedded_pane_clarifier(hint,
 * desktop_terminal) -> owned char*, cupping the returned string. Input is
 * "hint|||desktop(0/1)". Twin sets HERMES_DESKTOP_TERMINAL per scenario and
 * calls the real _tui_embedded_pane_clarifier(hint). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern char *agent_system_prompt_u_tui_embedded_pane_clarifier(const char *hint, int desktop_terminal);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <hint|||desktop>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[8192];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *sep = strstr(buf, "|||");
        const char *hint = buf;
        int desktop = 0;
        if (sep) { *sep = '\0'; desktop = (sep[3] == '1'); }
        WCUP_BEGIN("agent_system_prompt_u_tui_embedded_pane_clarifier");
        fflush(stdout);
        char *r = agent_system_prompt_u_tui_embedded_pane_clarifier(hint, desktop);
        WCUP_VALSTR(r ? r : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("agent_system_prompt_u_tui_embedded_pane_clarifier");
        free(r);
    }
    return 0;
}
