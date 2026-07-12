/* Slermes C port — agent/agent_runtime_helpers.py:intent_ack_continuation_mode */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* PoP: agent_intent_ack_continuation_mode @ agent/agent_runtime_helpers.py:intent_ack_continuation_mode */
void agent_intent_ack_continuation_mode(const char *mode, const char *api_mode, const char *model, char *out, size_t outsz)
{
    if (outsz == 0) return;
    /* mode may be NULL (treated as "auto"), a bool-ish string, or a list joined by ';'. */
    if (!mode || !*mode || strcmp(mode, "auto") == 0) {
        /* auto or unrecognised -> codex_only iff api_mode == codex_responses */
        const char *r = (api_mode && strcmp(api_mode, "codex_responses") == 0) ? "codex_only" : "off";
        snprintf(out, outsz, "%s", r);
        return;
    }
    /* true/always/yes/on -> all */
    if (strcmp(mode, "true") == 0 || strcmp(mode, "always") == 0 ||
        strcmp(mode, "yes") == 0 || strcmp(mode, "on") == 0) {
        snprintf(out, outsz, "all"); return;
    }
    /* false/never/no/off -> off */
    if (strcmp(mode, "false") == 0 || strcmp(mode, "never") == 0 ||
        strcmp(mode, "no") == 0 || strcmp(mode, "off") == 0) {
        snprintf(out, outsz, "off"); return;
    }
    /* list -> all if any element (case-insensitive) is a substring of model, else off */
    char mdl[1024];
    size_t n = 0;
    for (const char *p = model ? model : ""; *p && n + 1 < sizeof(mdl); p++) mdl[n++] = (char)tolower((unsigned char)*p);
    mdl[n] = '\0';
    /* split mode on ';' */
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", mode);
    char *tok = strtok(buf, ";");
    while (tok) {
        char low[512];
        size_t k = 0;
        for (char *q = tok; *q && k + 1 < sizeof(low); q++) low[k++] = (char)tolower((unsigned char)*q);
        low[k] = '\0';
        if (low[0] && strstr(mdl, low)) { snprintf(out, outsz, "all"); return; }
        tok = strtok(NULL, ";");
    }
    snprintf(out, outsz, "off");
}
