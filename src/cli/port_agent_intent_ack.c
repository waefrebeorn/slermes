/*
 * port_agent_intent_ack.c — C port of the two intent-ack continuation
 * helpers in agent/agent_runtime_helpers.py.
 *
 * The Python versions take the `agent` object and read attributes off it.
 * We port them as taking the three resolved inputs explicitly (mode,
 * api_mode, model) so they are pure and unit-testable. Faithful to
 * intent_ack_continuation_mode / intent_ack_continuation_enabled.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* PoP: intent_ack_continuation_mode @ agent/agent_runtime_helpers.py:intent_ack_continuation_mode */
/* Returns malloc'd mode string ("off" | "codex_only" | "all"). Caller frees. */
char *intent_ack_continuation_mode(const char *mode, const char *api_mode,
                                   const char *model) {
    /* First resolve mode to a canonical string if it's a truthy/falsy str. */
    char mlow[256];
    mlow[0] = '\0';
    if (mode) {
        size_t i = 0;
        for (; mode[i] && i + 1 < sizeof(mlow); i++)
            mlow[i] = (char)tolower((unsigned char)mode[i]);
        mlow[i] = '\0';
    }
    int is_true = (mode && (strcmp(mode, "1") == 0 || strcmp(mlow, "true") == 0 ||
                            strcmp(mlow, "always") == 0 || strcmp(mlow, "yes") == 0 ||
                            strcmp(mlow, "on") == 0));
    int is_false = (mode && (strcmp(mode, "0") == 0 || strcmp(mlow, "false") == 0 ||
                             strcmp(mlow, "never") == 0 || strcmp(mlow, "no") == 0 ||
                             strcmp(mlow, "off") == 0));

    /* mode as a list -> "all" if any element is a substring of model (lowercased) */
    /* We accept a ';'-separated list string as the Python list form. */
    if (mode && strchr(mode, ';')) {
        char mdl[1024];
        size_t k = 0;
        for (; model && model[k] && k + 1 < sizeof(mdl); k++)
            mdl[k] = (char)tolower((unsigned char)model[k]);
        mdl[k] = '\0';
        char *copy = strdup(mode);
        char *tok = strtok(copy, ";");
        int hit = 0;
        while (tok) {
            char tl[256]; size_t j = 0;
            for (; tok[j] && j + 1 < sizeof(tl); j++) tl[j] = (char)tolower((unsigned char)tok[j]);
            tl[j] = '\0';
            if (tl[0] && strstr(mdl, tl)) { hit = 1; break; }
            tok = strtok(NULL, ";");
        }
        free(copy);
        return strdup(hit ? "all" : "off");
    }

    if (is_true) return strdup("all");
    if (is_false) return strdup("off");

    /* "auto" or unrecognised -> historical codex-only behavior */
    char am[64];
    size_t a = 0;
    for (; api_mode && api_mode[a] && a + 1 < sizeof(am); a++)
        am[a] = (char)tolower((unsigned char)api_mode[a]);
    am[a] = '\0';
    return strdup(strcmp(am, "codex_responses") == 0 ? "codex_only" : "off");
}

/* PoP: intent_ack_continuation_enabled @ agent/agent_runtime_helpers.py:intent_ack_continuation_enabled */
bool intent_ack_continuation_enabled(const char *mode, const char *api_mode,
                                     const char *model) {
    char *m = intent_ack_continuation_mode(mode, api_mode, model);
    int on = m && strcmp(m, "off") != 0;
    free(m);
    return on ? true : false;
}
