/*
 * gateway_reply.c - faithful C11 port of pure helpers from upstream Hermes
 * acp_adapter/gateway_reply.py (gate w11_gatewayrep_9b4876).
 *
 * PoP: w11_gatewayrep_9b4876_text @ acp_adapter/gateway_reply.py:_text
 * PoP: w11_gatewayrep_9b4876_close_segment @ acp_adapter/gateway_reply.py:GatewayReplyMixin._close_segment
 * PoP: w11_gatewayrep_9b4876_final_remainder @ acp_adapter/gateway_reply.py:GatewayReplyMixin._final_remainder
 * PoP (helper): owed_reply @ hermes_cli/gateway_replay_gap.py:owed_reply
 *
 * Async update projection (session updates, tool cards) needs live ACP conn
 * objects and is intentionally not ported.
 */
#include "noc_w11_gatewayrep_9b4876.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INTERRUPT_PREFIX "Operation interrupted: waiting for model response ("
#define RESUMED_MARKER "\n[output resumed]\n"

static int py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *str_dup_n(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static char *str_dup(const char *s) { return str_dup_n(s ? s : "", strlen(s ? s : "")); }

static char *py_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && py_isspace((unsigned char)p[a])) a++;
    while (b > a && py_isspace((unsigned char)p[b - 1])) b--;
    return str_dup_n(p + a, b - a);
}

static char *py_rstrip(const char *s) {
    const char *p = s ? s : "";
    size_t b = strlen(p);
    while (b > 0 && py_isspace((unsigned char)p[b - 1])) b--;
    return str_dup_n(p, b);
}

static int startswith(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* set {s.strip() for s in shown} contains candidate? */
static int stripped_set_contains(const json_t *shown, const char *candidate) {
    if (!shown || shown->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < shown->c.count; i++) {
        json_t *v = shown->c.items[i];
        const char *s = (v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : "";
        char *t = py_strip(s);
        int hit = t && strcmp(t, candidate) == 0;
        free(t);
        if (hit) return 1;
    }
    return 0;
}

/* owed_reply(shown, final) from gateway_replay_gap.py. */
static char *owed_reply(const char *shown, const char *final) {
    const char *head = shown;
    if (startswith(head, "\n\n")) head += 2;
    char *rstripped = py_rstrip(head); /* Python: head.rstrip() (right only) */
    const char *prefixes[2] = { head, rstripped };
    for (int i = 0; i < 2; i++) {
        if (rstripped && prefixes[i] && startswith(final, prefixes[i])) {
            size_t n = strlen(prefixes[i]);
            char *out = str_dup_n(final + n, strlen(final) - n);
            free(rstripped);
            return out;
        }
    }
    free(rstripped);
    size_t a = strlen(RESUMED_MARKER), b = strlen(final);
    char *out = (char *)malloc(a + b + 1);
    if (!out) return NULL;
    memcpy(out, RESUMED_MARKER, a);
    memcpy(out + a, final, b);
    out[a + b] = '\0';
    return out;
}

char *w11_gatewayrep_9b4876_text(const json_t *payload) {
    json_t *v = payload ? json_obj_get(payload, "text") : NULL;
    if (v && v->type == JSON_STRING) return str_dup(v->str_val ? v->str_val : "");
    return str_dup("");
}

json_t *w11_gatewayrep_9b4876_close_segment(const char *streamed, const json_t *shown) {
    json_t *out = json_array();
    if (!out) return NULL;
    if (shown && shown->type == JSON_ARRAY) {
        for (size_t i = 0; i < shown->c.count; i++)
            json_append(out, json_copy(shown->c.items[i]));
    }
    char *t = py_strip(streamed);
    int keep = t && *t;
    free(t);
    if (keep) json_append(out, json_string(streamed ? streamed : ""));
    return out;
}

char *w11_gatewayrep_9b4876_final_remainder(const char *text, const char *streamed,
                                            const json_t *shown, const char *cut,
                                            const char *outcome, bool response_reused,
                                            bool has_receipt) {
    const char *t = text ? text : "";
    const char *s = streamed ? streamed : "";
    int cut_present = cut != NULL;

    if (!t[0] || ((outcome && strcmp(outcome, "cancelled") == 0) && startswith(t, INTERRUPT_PREFIX)))
        return str_dup("");

    char *stripped_text = py_strip(t);

    if (response_reused) {
        if (cut_present || has_receipt) {
            char *stripped_streamed = py_strip(s);
            int hit = stripped_set_contains(shown, stripped_text) ||
                      (stripped_streamed && strcmp(stripped_text, stripped_streamed) == 0);
            free(stripped_streamed);
            if (hit) { free(stripped_text); return str_dup(""); }
        } else {
            if (s[0]) {
                if (strstr(s, stripped_text) != NULL) { free(stripped_text); return str_dup(""); }
            } else if (shown && shown->type == JSON_ARRAY) {
                for (size_t i = 0; i < shown->c.count; i++) {
                    json_t *v = shown->c.items[i];
                    const char *sv = (v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : "";
                    if (strcmp(sv, stripped_text) == 0) { free(stripped_text); return str_dup(""); }
                }
            }
        }
    }

    if (cut_present) {
        int not_cut = (*cut == '\0');
        if (not_cut && stripped_set_contains(shown, stripped_text)) {
            free(stripped_text);
            return str_dup("");
        }
        char *r = owed_reply(cut, t);
        free(stripped_text);
        return r;
    }

    if (startswith(t, s)) {
        size_t slen = strlen(s);
        if (!s[0] && stripped_set_contains(shown, stripped_text)) {
            free(stripped_text);
            return str_dup("");
        }
        free(stripped_text);
        return str_dup_n(t + slen, strlen(t) - slen);
    }

    char *stripped_streamed = py_strip(s);
    int eq = stripped_streamed && strcmp(stripped_text, stripped_streamed) == 0;
    free(stripped_streamed);
    free(stripped_text);
    if (eq) return str_dup("");

    size_t a = strlen("\n\n"), b = strlen(t);
    char *out = (char *)malloc(a + b + 1);
    if (!out) return NULL;
    memcpy(out, "\n\n", a);
    memcpy(out + a, t, b);
    out[a + b] = '\0';
    return out;
}
