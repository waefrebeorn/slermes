/* gate: cron_suggestions_py__dismiss_suggestion — traced copy of cron_sugg_dismiss */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cron_sugg_dismiss(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cron_sugg_dismiss");
    bool result = cron_sugg_dismiss(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cron_sugg_dismiss");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_sugg_dismiss <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
