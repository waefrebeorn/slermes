/*
 * turn_iteration_prep.c — faithful C11 port of agent/turn_iteration_prep.py.
 * Gate w11_turniterat_f63754.
 *
 * PoP: w11_turniterat_f63754_anchors_current_turn @ agent/turn_iteration_prep.py:_anchors_current_turn
 * PoP: w11_turniterat_f63754_previous_tool_round @ agent/turn_iteration_prep.py:_previous_tool_round
 * PoP: w11_turniterat_f63754_neutralise_replay_echo_ghosts @ agent/turn_iteration_prep.py:_neutralise_replay_echo_ghosts
 */
#include "hermes_json.h"
#include "noc_w11_turniterat_f63754.h"
#include <stdlib.h>
#include <string.h>

#define MAX_MAP 512

static const char *jstr(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_get_str(o, k, "") : "";
}

static char *py_strip(const char *s) {
    size_t a = 0, b;
    char *out;
    if (!s) return NULL;
    b = strlen(s);
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' ||
                     s[a] == '\r' || s[a] == '\v' || s[a] == '\f')) a++;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\n' ||
                     s[b-1] == '\r' || s[b-1] == '\v' || s[b-1] == '\f')) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    if (b > a) memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* ---- _anchors_current_turn ---- */
int w11_turniterat_f63754_anchors_current_turn(
    const json_t *messages, const char *idx_s, const char *user_message, const json_t *view) {
    long long idx;
    json_t *msg;
    const char *content;
    if (!messages || messages->type != JSON_ARRAY || !idx_s) return 0;
    if (!strcmp(idx_s, "true")) idx = 1;
    else if (!strcmp(idx_s, "false")) idx = 0;
    else idx = strtoll(idx_s, NULL, 10);
    if (idx < 0 || idx >= (long long)messages->c.count) return 0;
    msg = messages->c.items[idx];
    if (!msg || msg->type != JSON_OBJECT || strcmp(jstr(msg, "role"), "user")) return 0;
    content = json_get_str(msg, "content", "");
    if (user_message && !strcmp(content, user_message)) return 1;
    /* user_originated_turn_view(msg) seam, consulted only after the verbatim check */
    if (view && view->type == JSON_OBJECT) {
        const char *vc = json_get_str(view, "content", "");
        if (user_message && !strcmp(vc, user_message)) return 1;
    }
    return 0;
}

/* ---- _previous_tool_round ---- */
json_t *w11_turniterat_f63754_previous_tool_round(const json_t *messages) {
    json_t *out = json_array();
    size_t n, rev, i;
    if (!messages || messages->type != JSON_ARRAY) return out;
    n = messages->c.count;
    for (rev = 0; rev < n; rev++) {
        json_t *m = messages->c.items[n - 1 - rev];
        json_t *tcs;
        const char *ids[MAX_MAP];
        json_t *vals[MAX_MAP];
        size_t mcount = 0, j;
        if (!m || m->type != JSON_OBJECT) continue;
        if (strcmp(jstr(m, "role"), "assistant")) continue;
        tcs = json_obj_get(m, "tool_calls");
        if (!tcs || tcs->type != JSON_ARRAY || tcs->c.count == 0) continue;
        /* results_by_id: tool rows immediately after this assistant row; last wins */
        for (i = n - rev; i < n && mcount < MAX_MAP; i++) {
            json_t *tm = messages->c.items[i];
            const char *tcid;
            if (!tm || tm->type != JSON_OBJECT || strcmp(jstr(tm, "role"), "tool")) break;
            tcid = json_get_str(tm, "tool_call_id", "");
            if (tcid && *tcid) {
                ids[mcount] = tcid;
                vals[mcount] = json_obj_get(tm, "content")
                               ? json_obj_get(tm, "content") : json_string("");
                mcount++;
            }
        }
        for (i = 0; i < tcs->c.count; i++) {
            json_t *tc = tcs->c.items[i], *fn, *item;
            const char *tc_id;
            if (!tc || tc->type != JSON_OBJECT) continue;
            fn = json_obj_get(tc, "function");
            if (!fn || fn->type != JSON_OBJECT) continue;
            item = json_object();
            json_set_hdr(item, "name", json_get_str(fn, "name", ""));
            tc_id = json_get_str(tc, "id", "");
            /* reverse scan so the last tool row for an id wins (dict overwrite) */
            {
                json_t *found = NULL;
                for (j = mcount; j-- > 0;) if (!strcmp(ids[j], tc_id)) { found = vals[j]; break; }
                json_set(item, "result", found ? json_copy(found) : json_null());
            }
            {
                json_t *a = json_obj_get(fn, "arguments");
                json_set(item, "arguments", a ? json_copy(a) : json_null());
            }
            json_append(out, item);
        }
        return out;
    }
    return out;
}

/* ---- _neutralise_replay_echo_ghosts ---- */
json_t *w11_turniterat_f63754_neutralise_replay_echo_ghosts(
    const json_t *seq, const char *scaffold_marker, const char *legacy_placeholder,
    const char *current_placeholder) {
    json_t *out = json_array(), *r;
    int neutralised = 0;
    size_t i;
    if (seq && seq->type == JSON_ARRAY) {
        for (i = 0; i < seq->c.count; i++) {
            json_t *m = seq->c.items[i];
            int hazard = 0;
            if (m && m->type == JSON_OBJECT
                    && !strcmp(json_get_str(m, "display_kind", ""), "hidden")
                    && !strcmp(json_get_str(m, "role", ""), "assistant")) {
                static const char *const keys[] = {"content", "api_content"};
                size_t k;
                for (k = 0; k < 2; k++) {
                    json_t *v = json_obj_get(m, keys[k]);
                    char *s;
                    if (!v || v->type != JSON_STRING || !v->str_val) continue;
                    s = py_strip(v->str_val); /* copy out before any mutation */
                    if (s && (!strcmp(s, scaffold_marker) || !strcmp(s, legacy_placeholder))) hazard = 1;
                    free(s);
                    if (hazard) break;
                }
            }
            if (hazard) {
                json_t *nm = json_copy(m);
                json_set_hdr(nm, "content", "");
                json_set_hdr(nm, "api_content", current_placeholder ? current_placeholder : "");
                json_append(out, nm);
                neutralised++;
            } else {
                json_append(out, json_copy(m));
            }
        }
    }
    r = json_object();
    json_set(r, "messages", out);
    json_set(r, "neutralised", json_number(neutralised));
    return r;
}
