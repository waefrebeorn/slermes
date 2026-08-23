/* gate: display_config_normalise — traced wrapper around the REAL
 * src/gateway/display_config.c::normalise_display_value (linked via extra_src).
 * LIVE twin: gateway/display_config.py::_normalise.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: setting == "tool_progress"
 *   B2: setting in boolean-ish set
 *   B3: setting == "tool_progress_grouping"
 *   B4: setting == "reasoning_style"
 *   B5: setting == "tool_preview_length"
 *   V : normalised result string (from the REAL C port)
 *
 * Contract: string-input path only (the agreed sub-contract; see notes).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

char *normalise_display_value(const char *setting, const char *value);

static const char *const BOOL_SETTINGS[] = {
    "show_reasoning", "streaming", "interim_assistant_messages",
    "long_running_notifications", "busy_ack_detail", "cleanup_progress", NULL
};

static void trace_one(const char *setting, const char *value) {
    WCUP_BEGIN("normalise");
    int b1 = strcmp(setting, "tool_progress") == 0;
    WCUP_BRANCH(b1);
    int is_bool = 0;
    if (!b1) {
        for (int i = 0; BOOL_SETTINGS[i]; i++)
            if (strcmp(setting, BOOL_SETTINGS[i]) == 0) { is_bool = 1; break; }
        WCUP_BRANCH(is_bool);   /* B2 */
        if (!is_bool) {
            WCUP_BRANCH(strcmp(setting, "tool_progress_grouping") == 0); /* B3 */
            WCUP_BRANCH(strcmp(setting, "reasoning_style") == 0);        /* B4 */
            WCUP_BRANCH(strcmp(setting, "tool_preview_length") == 0);    /* B5 */
        }
    }
    char *res = normalise_display_value(setting, value);
    WCUP_VALSTR(res ? res : "");
    free(res);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("normalise");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalise_display_value <setting|value>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *pair = strdup(argv[i]);
        char *bar = strchr(pair, '|');
        if (!bar) { fprintf(stderr, "bad pair %s\n", argv[i]); free(pair); return 2; }
        *bar = '\0';
        trace_one(pair, bar + 1);
        free(pair);
    }
    return 0;
}
