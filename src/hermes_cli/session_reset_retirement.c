/*
 * session_reset_retirement.c - C11 port of upstream
 * hermes_cli/session_reset_retirement.py (gate w24_sessionres_41c66a).
 *
 * PoP: w24_sessionres_41c66a_retired_reset_policy @ hermes_cli/session_reset_retirement.py:retired_reset_policy
 * PoP: w24_sessionres_41c66a_format_notice @ hermes_cli/session_reset_retirement.py:format_notice
 * PoP: w24_sessionres_41c66a_constants @ hermes_cli/session_reset_retirement.py:PLUGIN_NAME
 *
 * Ported: the whole module bar one call.  retired_reset_policy reads only the
 * config mapping it is handed (top-level block first, then the gateway: form
 * the pre-removal loader also accepted) and format_notice is a pure f-string.
 * reset_plugin_enabled() is the seam: it discovers plugins in this process and
 * returns nothing a value decides, so it is not ported.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_sessionres_41c66a.h"

static const char *const SR_PLUGIN_NAME = "hermes-session-reset-policy";
static const char *const SR_TIMED_MODES[W24_SESSIONRES_41C66A_TIMED_MODES] = {
    "idle", "daily", "both",
};

/* str(v).strip().lower() for the shapes a YAML ``mode:`` can carry.  A value
 * whose Python str() is not one of the timed modes cannot match, so only
 * strings are lowered; everything else becomes "" and falls through. */
static void sr_mode(const json_t *block, char *buf, size_t cap) {
    const json_t *v;
    size_t i, o = 0;

    buf[0] = '\0';
    if (!block || block->type != JSON_OBJECT) return;
    v = json_obj_get(block, "mode");
    if (!v || v->type != JSON_STRING || !v->str_val) return;
    /* .strip(): Python's str.strip() with no argument drops all whitespace. */
    {
        const char *s = v->str_val;
        size_t n = strlen(s), a = 0, b = n;
        while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r' ||
                         s[a] == '\v' || s[a] == '\f')) a++;
        while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' ||
                         s[b - 1] == '\r' || s[b - 1] == '\v' || s[b - 1] == '\f')) b--;
        for (i = a; i < b && o + 1 < cap; i++) {
            char c = s[i];
            buf[o++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
        }
        buf[o] = '\0';
    }
}

static int sr_timed(const char *mode) {
    size_t i;
    for (i = 0; i < W24_SESSIONRES_41C66A_TIMED_MODES; i++)
        if (strcmp(mode, SR_TIMED_MODES[i]) == 0) return 1;
    return 0;
}

json_t *w24_sessionres_41c66a_retired_reset_policy(const json_t *config) {
    const json_t *gateway, *block;
    char mode[64];
    json_t *out;

    if (!config || config->type != JSON_OBJECT) return json_null();
    gateway = json_obj_get(config, "gateway");

    block = json_obj_get(config, "session_reset");
    if (block && block->type == JSON_OBJECT) {
        sr_mode(block, mode, sizeof(mode));
        if (sr_timed(mode)) {
            out = json_array();
            json_append(out, json_string("session_reset"));
            json_append(out, json_string(mode));
            return out;
        }
    }
    block = (gateway && gateway->type == JSON_OBJECT)
                ? json_obj_get(gateway, "session_reset") : NULL;
    if (block && block->type == JSON_OBJECT) {
        sr_mode(block, mode, sizeof(mode));
        if (sr_timed(mode)) {
            out = json_array();
            json_append(out, json_string("gateway.session_reset"));
            json_append(out, json_string(mode));
            return out;
        }
    }
    return json_null();
}

/* str(v) for the f-string's two substitutions: a string as itself, None as
 * "None", so a caller that passes a non-string still renders like Python. */
static void sr_text(const json_t *v, char *buf, size_t cap) {
    if (cap == 0) return;
    buf[0] = '\0';
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
    case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
    case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val &&
            v->num_val > -1e16 && v->num_val < 1e16)
            snprintf(buf, cap, "%lld", (long long)v->num_val);
        else
            snprintf(buf, cap, "%.17g", v->num_val);
        return;
    default: {
        char *s = json_dumps(v, 0);
        snprintf(buf, cap, "%s", s ? s : "");
        free(s);
        return;
    }
    }
}

json_t *w24_sessionres_41c66a_format_notice(const json_t *path, const json_t *mode) {
    char p[1024], m[256];
    const char *move;
    size_t need;
    char *buf;
    json_t *out;

    sr_text(path, p, sizeof(p));
    sr_text(mode, m, sizeof(m));
    move = strcmp(p, "session_reset") == 0
               ? ""
               : " (move the block to top-level session_reset first)";
    need = strlen(p) + strlen(m) + strlen(move) + strlen(SR_PLUGIN_NAME) + 256;
    buf = (char *)malloc(need);
    if (!buf) return json_null();
    snprintf(buf, need,
             "%s.mode: %s is no longer applied: gateway conversations reset only on /new or "
             "/reset. To keep idle/daily resets, run `hermes plugins install %s`%s.",
             p, m, SR_PLUGIN_NAME, move);
    out = json_string(buf);
    free(buf);
    return out;
}

json_t *w24_sessionres_41c66a_constants(void) {
    json_t *o = json_object();
    json_t *modes = json_array();
    size_t i;
    for (i = 0; i < W24_SESSIONRES_41C66A_TIMED_MODES; i++)
        json_append(modes, json_string(SR_TIMED_MODES[i]));
    json_set(o, "plugin_name", json_string(SR_PLUGIN_NAME));
    json_set(o, "timed_modes", modes);
    return o;
}
