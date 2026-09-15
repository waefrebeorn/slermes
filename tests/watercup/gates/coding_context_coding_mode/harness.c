/* gate: coding_context_coding_mode — links the REAL C port
 * src/agent/coding_ctx_gaps.c::coding_ctx_mode (post-parity-fix: reads
 * agent.coding_context and normalizes to auto/focus/on/off like LIVE).
 * LIVE twin: agent/coding_context.py::_coding_mode (config dict passed
 * explicitly so the readonly config load is never on the traced path).
 *
 * Input argv: one config JSON object per input.
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: config is an object        B2: agent sub-object present
 *   B3: focus-set                  B4: on-set   B5: off-set
 *   VSTR(normalized mode)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "watercup.h"

const char *coding_ctx_mode(json_t *config);

static void run_one(const char *cfg_json) {
    json_t *cfg = cfg_json ? json_parse(cfg_json, NULL) : NULL;
    bool obj = cfg && cfg->type == JSON_OBJECT;
    json_t *agent = obj ? json_obj_get(cfg, "agent") : NULL;
    bool has_agent = agent && agent->type == JSON_OBJECT;

    /* mirror the normalization for tracing */
    const char *raw = "auto";
    if (has_agent) {
        json_t *cc = json_obj_get(agent, "coding_context");
        if (cc && cc->type == JSON_STRING && json_string_value(cc))
            raw = json_string_value(cc);
    }
    char mode[64];
    size_t i = 0;
    for (; raw[i] && i < sizeof(mode) - 1; i++)
        mode[i] = (char)tolower((unsigned char)raw[i]);
    /* LIVE str(raw).strip() */
    {
        size_t len = i;
        char *s = mode;
        while (len > 0 && (s[0] == ' ' || s[0] == '\t' || s[0] == '\n')) { memmove(s, s + 1, len); len--; }
        while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\n')) len--;
        s[len] = '\0';
    }
    bool focus = !strcmp(mode, "focus") || !strcmp(mode, "strict") || !strcmp(mode, "lean");
    bool on = !strcmp(mode, "on") || !strcmp(mode, "true") || !strcmp(mode, "yes") ||
              !strcmp(mode, "1") || !strcmp(mode, "always");
    bool off = !strcmp(mode, "off") || !strcmp(mode, "false") || !strcmp(mode, "no") ||
               !strcmp(mode, "0") || !strcmp(mode, "never");

    WCUP_BEGIN("_coding_mode");
    WCUP_BRANCH(obj);                                   /* B1 */
    WCUP_BRANCH(has_agent);                             /* B2 */
    WCUP_BRANCH(focus);                                 /* B3 */
    WCUP_BRANCH(on);                                    /* B4 */
    WCUP_BRANCH(off);                                   /* B5 */

    const char *r = coding_ctx_mode(cfg);
    WCUP_VALSTR(r);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    if (cfg) json_free(cfg);
    WCUP_END("_coding_mode");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <config_json...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
