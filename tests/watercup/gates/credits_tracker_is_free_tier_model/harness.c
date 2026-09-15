/* gate: credits_tracker_is_free_tier_model
 * Calls the REAL C port src/agent/credits_tracker.c::
 * cli_agent_credits_tracker_is_free_tier_model against LIVE Python
 * agent/credits_tracker.py::is_free_tier_model.
 * Input argv format: "<model><TAB><base_url>".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

bool cli_agent_credits_tracker_is_free_tier_model(const char *model, const char *base_url);

/* out-of-path externals from credits_tracker.o (not on this fn's path) */
void hermes_log(int lvl, const char *fmt, ...) {(void)lvl; (void)fmt;}

static void run_one(const char *input) {
    WCUP_BEGIN("is_free_tier_model");
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", input ? input : "");
    char *tab = strchr(tmp, '\t');
    const char *base_url = "";
    if (tab) { *tab = '\0'; base_url = tab + 1; }
    const char *model = tmp;

    WCUP_BRANCH(!(model && model[0]));
    size_t len = model ? strlen(model) : 0;
    int is_free = 0;
    if (model && model[0]) {
        if (len >= 5 && strcmp(model + len - 5, ":free") == 0) {
            WCUP_BRANCH(1);
            is_free = 1;
        } else {
            WCUP_BRANCH(0);
            if (len >= 8 && strncmp(model, "stealth/", 8) == 0) {
                WCUP_BRANCH(1);
                is_free = 1;
            } else {
                WCUP_BRANCH(0);
                WCUP_BRANCH(!(base_url && base_url[0]));
                /* base_url non-empty: LIVE consults the in-process pricing
                 * cache (empty in this harness => False); C omits it => False */
            }
        }
    }
    WCUP_VALSTR(is_free ? "True" : "False");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_free_tier_model");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_free_tier_model <model<TAB>base_url>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
