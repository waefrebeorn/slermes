/* gate: port__agent_display_py___display_url -> real _display_url (JSON value) -> string. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern char *agent_display_u_display_url(const char *arg);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("agent_display_u_display_url");
        fflush(stdout);
        char *r = agent_display_u_display_url(argv[i]);
        WCUP_VALSTR(r ? r : "");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("agent_display_u_display_url");
        free(r);
    }
    return 0;
}
