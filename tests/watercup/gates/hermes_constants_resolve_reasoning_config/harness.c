/* gate: hermes_constants_resolve_reasoning_config — traced copy of
 * src/gateway/hermes_constants_reasoning.c::reasoning_resolve_config
 * (LIVE twin: hermes_constants.py::resolve_reasoning_config).
 *
 * The heavy lifting (tolerant per-model override lookup incl. canonical
 * model-name variants) runs via the LINKED REAL PORT
 * (extra_src: src/gateway/hermes_constants_reasoning.c); this harness adds
 * WCUP rungs around the resolution chokepoint only, plus a verbatim copy of
 * reasoning_parse_effort + helpers from src/gateway/slash_commands.c.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B0: cfg is an object
 *   B1: 'agent' section is an object
 *   B2: model arg empty AND cfg present -> derive from cfg['model']
 *       V : effective model
 *   B3: reasoning_overrides present & object
 *     B4: per-model lookup hit
 *         V : resolved config (encoded) on every exit
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "libjson/json.h"
#include "watercup.h"

/* ---- verbatim leaf parser from src/gateway/slash_commands.c ---- */
static const char *const VALID_REASONING_EFFORTS[] = {
    "minimal", "low", "medium", "high", "xhigh", "max", "ultra", NULL
};
static char *strip_dup(const char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}
static void ascii_lower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}
json_t *reasoning_parse_effort(const char *effort, bool effort_is_bool_false) {
    if (effort_is_bool_false) {
        json_t *o = json_object();
        json_set(o, "enabled", json_bool(false));
        return o;
    }
    if (!effort) return NULL;
    char *low = strip_dup(effort);
    if (!low || low[0] == '\0') { free(low); return NULL; }
    ascii_lower(low);
    if (strcmp(low, "none") == 0 || strcmp(low, "false") == 0 ||
        strcmp(low, "disabled") == 0) {
        free(low);
        json_t *o = json_object();
        json_set(o, "enabled", json_bool(false));
        return o;
    }
    for (int i = 0; VALID_REASONING_EFFORTS[i]; i++) {
        if (strcmp(low, VALID_REASONING_EFFORTS[i]) == 0) {
            json_t *o = json_object();
            json_set(o, "enabled", json_bool(true));
            json_set(o, "effort", json_string(low));
            free(low);
            return o;
        }
    }
    free(low);
    return NULL;
}

/* ---- real port functions (linked) ---- */
json_t *reasoning_resolve_per_model(const char *model, const json_t *overrides);

/* encode a resolved config the same way both sides do */
static void encode_result(json_t *r, char *buf, size_t cap) {
    if (!r) { snprintf(buf, cap, "<none>"); return; }
    json_t *en = json_obj_get(r, "enabled");
    json_t *ef = json_obj_get(r, "effort");
    if (en && en->type == JSON_BOOL && !en->bool_val)
        snprintf(buf, cap, "disabled");
    else if (ef && ef->type == JSON_STRING)
        snprintf(buf, cap, "enabled:%s", ef->str_val);
    else
        snprintf(buf, cap, "<none>");
}

static int wc_resolve(const char *model_arg, const char *cfg_json) {
    char enc[128];
    json_t *result;
    WCUP_BEGIN("reasoning_resolve_config");
    {
        char *jerr = NULL;
        json_t *cfg = json_parse(cfg_json ? cfg_json : "", &jerr);
        json_t *use_cfg = (cfg && cfg->type == JSON_OBJECT) ? cfg : NULL;
        WCUP_BRANCH(use_cfg != NULL);                                  /* B0 */
        json_t *agent_cfg = use_cfg ? json_obj_get(use_cfg, "agent") : NULL;
        if (agent_cfg && agent_cfg->type != JSON_OBJECT) agent_cfg = NULL;
        WCUP_BRANCH(agent_cfg != NULL);                                /* B1 */

        char *derived = NULL;
        const char *eff_model = model_arg;
        if ((!model_arg || !model_arg[0]) && use_cfg) {
            /* derive_model_from_cfg (verbatim semantics, traced) */
            json_t *mc = json_obj_get(use_cfg, "model");
            const char *pick = "";
            char pickbuf[256];
            if (mc && mc->type == JSON_STRING) {
                const char *s = mc->str_val ? mc->str_val : "";
                while (*s && isspace((unsigned char)*s)) s++;
                const char *e = s + strlen(s);
                while (e > s && isspace((unsigned char)e[-1])) e--;
                snprintf(pickbuf, sizeof pickbuf, "%.*s", (int)(e - s), s);
                pick = pickbuf;
            } else if (mc && mc->type == JSON_OBJECT) {
                json_t *d = json_obj_get(mc, "default");
                const char *p2 = (d && d->type == JSON_STRING) ? d->str_val : NULL;
                if (!p2 || !p2[0]) {
                    json_t *m2 = json_obj_get(mc, "model");
                    p2 = (m2 && m2->type == JSON_STRING) ? m2->str_val : NULL;
                }
                p2 = p2 ? p2 : "";
                while (*p2 && isspace((unsigned char)*p2)) p2++;
                const char *e = p2 + strlen(p2);
                while (e > p2 && isspace((unsigned char)e[-1])) e--;
                snprintf(pickbuf, sizeof pickbuf, "%.*s", (int)(e - p2), p2);
                pick = pickbuf;
            }
            derived = strdup(pick);
            eff_model = derived;
            WCUP_BRANCH(1);                                            /* B2 */
        } else {
            WCUP_BRANCH(0);                                            /* B2 */
        }
        WCUP_VALSTR(eff_model ? eff_model : "");

        json_t *overrides = agent_cfg ? json_obj_get(agent_cfg, "reasoning_overrides") : NULL;
        WCUP_BRANCH(overrides && overrides->type == JSON_OBJECT);      /* B3 */
        result = NULL;
        if (overrides && overrides->type == JSON_OBJECT) {
            json_t *per = reasoning_resolve_per_model(eff_model, overrides);
            WCUP_BRANCH(per != NULL);                                  /* B4 */
            if (per) result = per;
        }

        if (!result) {
            /* global agent.reasoning_effort — raw value kept */
            json_t *effort = agent_cfg ? json_obj_get(agent_cfg, "reasoning_effort") : NULL;
            if (!effort)
                result = NULL;                       /* default "" -> parse -> None */
            else if (effort->type == JSON_BOOL)
                result = reasoning_parse_effort(NULL, effort->bool_val == false);
            else if (effort->type == JSON_STRING)
                result = reasoning_parse_effort(effort->str_val, false);
            else
                result = NULL;                       /* null/number -> None */
        }
        encode_result(result, enc, sizeof enc);
        WCUP_VALSTR(enc);
        free(derived);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("reasoning_resolve_config");
        if (cfg) json_free(cfg);
        return 0;
    }
}

int main(int argc, char **argv) {
    int i;
    if (argc < 3) {
        fprintf(stderr, "usage: %s reasoning_resolve_config <model|cfg-json> ...\n", argv[0]);
        return 2;
    }
    for (i = 2; i < argc; i++) {
        char *arg = strdup(argv[i]);
        char *bar = strchr(arg, '|');
        if (bar) { *bar = '\0'; bar++; }
        wc_resolve(arg, bar ? bar : "");
        free(arg);
    }
    return 0;
}
