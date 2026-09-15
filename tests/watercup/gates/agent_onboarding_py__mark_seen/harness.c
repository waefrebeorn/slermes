/* gate: agent_onboarding_py__mark_seen — traced copy of obd_mark_seen */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int obd_mark_seen(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("obd_mark_seen");
    int result = obd_mark_seen(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("obd_mark_seen");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s obd_mark_seen <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
