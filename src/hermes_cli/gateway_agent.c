/*
 * gateway_agent.c — C port of upstream Hermes hermes_cli/gateway_agent.py.
 *
 * PoP: w10_gatewayage_356e4e_effort_word @ hermes_cli/gateway_agent.py:_effort_word
 * PoP: w10_gatewayage_356e4e_review_summary_line @ hermes_cli/gateway_agent.py:review_summary_line
 * PoP: w10_gatewayage_356e4e_clarify_reply @ hermes_cli/gateway_agent.py:clarify_reply
 *
 * VOICE_STOPPED is mirrored as a module constant; the gateway/RPC seams in the
 * original are represented as explicit fixture inputs (reply / params).
 */
#include "noc_w10_gatewayage_356e4e.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* VOICE_STOPPED is mirrored as a module constant (unused by the pure helpers). */
static const char *const VOICE_STOPPED = "voice_stopped";
static const char *voice_stopped(void) { return VOICE_STOPPED; }

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}
static char *py_str(const json_t *v) {
    if (!v) return strdup("");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else snprintf(b, sizeof(b), "%g", v->num_val);
        return strdup(b);
    }
    return strdup("");
}
static char *strip(const char *s) {
    if (!s) return strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *o = (char *)malloc(n + 1); memcpy(o, s, n); o[n] = 0;
    return o;
}

/* json.dumps(v) with Python's default separators for the scalar/container cases
 * used by clarify_reply's answer values (strings/None are passed through). */
static void py_dump_into(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "null"); return; }
    if (v->type == JSON_STRING) { snprintf(buf, cap, "\"%s\"", v->str_val ? v->str_val : ""); return; }
    if (v->type == JSON_BOOL) { snprintf(buf, cap, "%s", v->bool_val ? "true" : "false"); return; }
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, cap, "%lld", (long long)v->num_val);
        else snprintf(buf, cap, "%g", v->num_val);
        return;
    }
    if (v->type == JSON_ARRAY) {
        size_t pos = 0;
        buf[pos++] = '[';
        for (size_t i = 0; i < json_len(v); i++) {
            if (i) { buf[pos++] = ','; buf[pos++] = ' '; }
            char tmp[512]; py_dump_into(json_get(v, i), tmp, sizeof(tmp));
            size_t l = strlen(tmp); if (pos + l + 2 >= cap) break;
            memcpy(buf + pos, tmp, l); pos += l;
        }
        buf[pos++] = ']'; buf[pos] = 0; return;
    }
    /* JSON_OBJECT */
    size_t pos = 0; buf[pos++] = '{';
    for (size_t i = 0; i < json_object_size(v); i++) {
        if (i) { buf[pos++] = ','; buf[pos++] = ' '; }
        const char *k = json_object_get_key_at(v, i);
        char tmp[512]; py_dump_into(json_object_get_at(v, i), tmp, sizeof(tmp));
        int n = snprintf(buf + pos, cap - pos, "\"%s\": %s", k ? k : "", tmp);
        if (n < 0 || (size_t)n >= cap - pos) break;
        pos += (size_t)n;
    }
    buf[pos++] = '}'; buf[pos] = 0;
}

json_t *w10_gatewayage_356e4e_effort_word(const json_t *config) {
    if (!json_is_object(config)) return json_null();
    const json_t *enabled = json_obj_get(config, "enabled");
    if (enabled && enabled->type == JSON_BOOL && !enabled->bool_val)
        return json_string("none");                     /* config.get("enabled") is False */
    const json_t *effort = json_obj_get(config, "effort");
    if (effort && effort->type == JSON_STRING && effort->str_val[0])
        return json_copy(effort);
    return json_null();
}

json_t *w10_gatewayage_356e4e_review_summary_line(const struct json_t *payload) {
    char *text = NULL;
    if (json_is_object(payload)) {
        const json_t *v = json_obj_get(payload, "text");
        if (truthy(v)) { char *raw = py_str(v); text = strip(raw); free(raw); }
        else text = strdup("");
    } else text = strdup("");
    if (!text[0]) { free(text); return json_null(); }
    char *out = (char *)malloc(strlen(text) + 3);
    snprintf(out, strlen(text) + 3, "  %s", text);
    free(text);
    json_t *r = json_string(out); free(out);
    return r;
}

json_t *w10_gatewayage_356e4e_clarify_reply(const struct json_t *reply, const struct json_t *params) {
    json_t *questions = json_array();
    const json_t *pq = json_obj_get(params, "questions");
    if (json_is_array(pq)) {
        for (size_t i = 0; i < json_len(pq); i++) {
            const json_t *q = json_get(pq, i);
            if (!json_is_object(q)) continue;
            const json_t *qid = json_obj_get(q, "qid");
            if (!truthy(qid)) continue;                  /* filter: isinstance(q,dict) and q.get("qid") */
            json_t *item = json_object();
            json_set(item, "qid", json_copy(qid));
            const json_t *question = json_obj_get(q, "question");
            json_set(item, "question", (question && truthy(question)) ? json_copy(question) : json_string(""));
            const json_t *choices = json_obj_get(q, "choices");
            json_set(item, "choices", json_is_array(choices) && truthy(choices) ? json_copy(choices) : json_array());
            json_set(item, "multi_select", json_bool(truthy(json_obj_get(q, "multi_select")) ? true : false));
            json_append(questions, item);
        }
    }
    /* callback is None (`reply == NULL`) or no questions -> cancelled */
    if (!reply || json_len(questions) == 0 || !json_is_object(reply)) {
        json_free(questions);
        json_t *out = json_object();
        json_set(out, "outcome", json_string("cancelled"));
        return out;
    }
    json_t *answers = json_object();
    const json_t *ra = json_obj_get(reply, "answers");
    if (json_is_object(ra)) {
        for (size_t i = 0; i < json_object_size(ra); i++) {
            const char *k = json_object_get_key_at(ra, i);
            const json_t *val = json_object_get_at(ra, i);
            if (val && (val->type == JSON_STRING || val->type == JSON_NULL)) {
                json_set(answers, k, json_copy(val));
            } else {
                char b[2048]; py_dump_into(val, b, sizeof(b));
                json_set(answers, k, json_string(b));
            }
        }
    }
    json_t *out = json_object();
    json_set(out, "answers", answers);
    const json_t *oc = json_obj_get(reply, "outcome");
    if (oc && oc->type == JSON_STRING &&
        (!strcmp(oc->str_val, "submitted") || !strcmp(oc->str_val, "timed_out") || !strcmp(oc->str_val, "cancelled")))
        json_set(out, "outcome", json_copy(oc));
    json_free(questions);
    return out;
}
