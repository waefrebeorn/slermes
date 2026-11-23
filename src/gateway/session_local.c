/*
 * session_local.c — C home for upstream Hermes gateway/session_local.py.
 *
 * PoP: w8_sessionloca_b67e17_effort_fields @ gateway/session_local.py:effort_fields
 * PoP: w8_sessionloca_b67e17_identity @ gateway/session_local.py:LocalSessionAdapter._identity
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "noc_w8_sessionloca_b67e17.h"

/* agent.reasoning_effort.EFFORT_LADDER (low -> high). */
static const char *const W8_EFFORT_LADDER[] = {
    "none", "minimal", "low", "medium", "high", "xhigh", "max", "ultra"
};
#define W8_LADDER_LEN (sizeof(W8_EFFORT_LADDER) / sizeof(W8_EFFORT_LADDER[0]))

static int w8_ladder_index(const char *s) {
    for (size_t i = 0; i < W8_LADDER_LEN; i++)
        if (!strcmp(s, W8_EFFORT_LADDER[i])) return (int)i;
    return -1;
}

/* Python str(value or '').strip().lower() for JSON string values. */
static void w8_requested(const char *effort, char *buf, size_t cap) {
    size_t n = strlen(effort), a = 0, b = n;
    while (a < b && isspace((unsigned char)effort[a])) a++;
    while (b > a && isspace((unsigned char)effort[b - 1])) b--;
    size_t j = 0;
    for (size_t i = a; i < b && j + 1 < cap; i++)
        buf[j++] = (char)tolower((unsigned char)effort[i]);
    buf[j] = '\0';
}

/* Faithful port of agent.reasoning_effort.clamp_effort with no overrides. */
static const char *w8_clamp_effort(const char *effort, const json_t *supported) {
    char req[128];
    w8_requested(effort, req, sizeof(req));
    if (!req[0] || !supported || supported->type != JSON_ARRAY || json_len(supported) == 0)
        return effort;
    char norm[64][128];
    int norm_n = 0;
    for (size_t i = 0; i < json_len(supported) && norm_n < 64; i++) {
        json_t *s = json_get(supported, i);
        if (!s || s->type != JSON_STRING) continue; /* fixtures use strings */
        char v[128];
        w8_requested(s->str_val, v, sizeof(v));
        if (w8_ladder_index(v) >= 0) strcpy(norm[norm_n++], v);
    }
    bool in_norm = false;
    for (int i = 0; i < norm_n; i++) if (!strcmp(norm[i], req)) in_norm = true;
    if (norm_n == 0 || in_norm) return effort;
    int ridx = w8_ladder_index(req);
    if (ridx < 0) return effort;
    int best = -1, best_idx = -1;
    for (int i = 0; i < norm_n; i++) {
        if (!strcmp(norm[i], "none")) continue;
        int idx = w8_ladder_index(norm[i]);
        if (idx < ridx && idx > best_idx) { best_idx = idx; best = i; }
    }
    if (best < 0) { /* min(candidates, key=index) */
        for (int i = 0; i < norm_n; i++) {
            if (!strcmp(norm[i], "none")) continue;
            int idx = w8_ladder_index(norm[i]);
            if (best < 0 || idx < best_idx) { best_idx = idx; best = i; }
        }
    }
    return best < 0 ? effort : norm[best];
}

json_t *w8_sessionloca_b67e17_effort_fields(const json_t *config,
        const json_t *tier, const json_t *model, const json_t *provider,
        const json_t *base_url, const json_t *api_mode,
        const json_t *supported_efforts, const json_t *static_tiers,
        const json_t *fast_override) {
    (void)model; (void)provider; (void)base_url; (void)api_mode; /* consumed by the injected seams */

    const char *effort = "";
    char ebuf[128];
    if (config && config->type == JSON_OBJECT) {
        json_t *en = json_obj_get(config, "enabled");
        if (en && en->type == JSON_BOOL && !en->bool_val) {
            effort = "none";
        } else {
            json_t *ev = json_obj_get(config, "effort");
            if (ev && ev->type == JSON_STRING && ev->str_val[0] != '\0') {
                snprintf(ebuf, sizeof(ebuf), "%s", ev->str_val);
                effort = ebuf;
            }
        }
    }

    const char *wire = "";
    static char wbuf[256];
    if (effort[0] && strcmp(effort, "none")) {
        const char *clamped = w8_clamp_effort(effort, supported_efforts);
        snprintf(wbuf, sizeof(wbuf), "%s", clamped ? clamped : "");
        wire = wbuf;
    }

    const char *t = (tier && tier->type == JSON_STRING) ? tier->str_val : "";
    bool in_static = false;
    if (static_tiers && static_tiers->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(static_tiers); i++) {
            json_t *s = json_get(static_tiers, i);
            if (s && s->type == JSON_STRING && !strcmp(s->str_val, t)) { in_static = true; break; }
        }
    }
    bool fast = in_static && !(fast_override && fast_override->type == JSON_NULL);

    json_t *out = json_object();
    json_set(out, "reasoning_effort", json_string(effort));
    json_set(out, "reasoning_effort_wire", json_string(wire));
    json_set(out, "service_tier", json_string(t));
    json_set(out, "fast", json_bool(fast));
    return out;
}

json_t *w8_sessionloca_b67e17_identity(const json_t *source) {
    static const char *const KEYS[] = {"platform", "chat_id", "user_id", "profile", "chat_type"};
    json_t *out = json_array();
    for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        json_t *v = (source && source->type == JSON_OBJECT) ? json_obj_get(source, KEYS[i]) : NULL;
        json_append(out, v ? json_copy(v) : json_null());
    }
    return out;
}
