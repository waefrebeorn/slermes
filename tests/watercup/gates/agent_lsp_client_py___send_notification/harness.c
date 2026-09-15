/* gate: agent_lsp_client_py___send_notification — traced copy of lspc_send_notification */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int lspc_send_notification(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("lspc_send_notification");
    int result = lspc_send_notification(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("lspc_send_notification");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lspc_send_notification <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
