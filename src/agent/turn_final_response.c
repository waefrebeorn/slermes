/*
 * turn_final_response.c — faithful C11 port of agent/turn_final_response.py.
 * Gate w11_turnfinal_45ceb6.
 *
 * PoP: w11_turnfinal_45ceb6_bare_marker_after_reply @ agent/turn_final_response.py:_bare_marker_after_reply
 * PoP: w11_turnfinal_45ceb6_absorb_continuation_fragments @ agent/turn_final_response.py:_absorb_continuation_fragments
 * PoP: w11_turnfinal_45ceb6_pop_ephemeral_scaffolding @ agent/turn_final_response.py:finish_text_response
 */
#include "hermes_json.h"
#include "noc_w11_turnfinal_45ceb6.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dup_cstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

/* ---- _bare_marker_after_reply ---- */
int w11_turnfinal_45ceb6_bare_marker_after_reply(
    int needs_reply, int truncated_parts, int promoted, int interrupt_requested,
    const char *reply, int reply_is_silence, int final_is_silence) {
    /* `getattr(agent, "_silence_marker_needs_reply", False) is not True` */
    if (needs_reply != 1 || truncated_parts || promoted || interrupt_requested) return 0;
    if (!reply || !*reply) return 0;                 /* bool(reply) */
    if (reply_is_silence) return 0;                  /* not is_intentional_silence_response(reply) */
    return final_is_silence ? 1 : 0;                 /* is_intentional_silence_response(stripped) */
}

/* ---- faithful port of conversation_loop._continuation_overlap_length ---- */
#define MIN_CONTINUATION_OVERLAP 32
static size_t continuation_overlap_length(const char *prev, const char *cont) {
    size_t lp = strlen(prev), lc = strlen(cont), i;
    size_t matched = 0;
    int *pi;
    if (lp < MIN_CONTINUATION_OVERLAP || lc < MIN_CONTINUATION_OVERLAP) return 0;
    pi = (int *)calloc(lc ? lc : 1, sizeof(int));
    if (!pi) return 0;
    for (i = 1; i < lc; i++) {
        while (matched && cont[i] != cont[matched]) matched = (size_t)pi[matched - 1];
        if (cont[i] == cont[matched]) { matched++; pi[i] = (int)matched; }
    }
    matched = 0;
    for (i = 0; i < lp; i++) {
        char ch = prev[i];
        while (matched && ch != cont[matched]) matched = (size_t)pi[matched - 1];
        if (ch == cont[matched]) {
            matched++;
            if (matched == lc) {
                if (i == lp - 1) { free(pi); return matched; }
                matched = (size_t)pi[matched - 1];
            }
        }
    }
    free(pi);
    return matched >= MIN_CONTINUATION_OVERLAP ? matched : 0;
}

typedef struct { char *s; size_t n, cap; } strbuf;
static void sb_init(strbuf *b) { b->s = NULL; b->n = b->cap = 0; }
static void sb_reserve(strbuf *b, size_t extra) {
    if (b->n + extra + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap * 2 : 64;
    while (nc < b->n + extra + 1) nc *= 2;
    char *p = (char *)realloc(b->s, nc);
    if (!p) return;
    b->s = p; b->cap = nc;
}
static void sb_addn(strbuf *b, const char *s, size_t n) {
    if (!n || !s) return;
    sb_reserve(b, n);
    if (!b->s) return;
    memcpy(b->s + b->n, s, n); b->n += n; b->s[b->n] = '\0';
}
static void sb_addc(strbuf *b, char c) { sb_reserve(b, 1); if (!b->s) return; b->s[b->n++] = c; b->s[b->n] = '\0'; }
static int ascii_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* ---- faithful port of conversation_loop._join_truncated_parts ---- */
static char *join_truncated_parts(const json_t *parts, const char *final_response) {
    strbuf joined;
    int previous_was_partial_stub = 0;
    size_t i, nparts = parts && parts->type == JSON_ARRAY ? parts->c.count : 0;
    sb_init(&joined);
    for (i = 0; i <= nparts; i++) {
        const char *raw = (i < nparts) ? json_get_str(parts->c.items[i] && parts->c.items[i]->type == JSON_OBJECT
                                                      ? parts->c.items[i] : NULL, "text", "")
                                       : (final_response ? final_response : "");
        int stub = 0;
        char *part;
        size_t plen, off = 0;
        if (i < nparts && parts->c.items[i] && parts->c.items[i]->type == JSON_OBJECT)
            stub = json_get_bool(parts->c.items[i], "is_partial_stub", false) ? 1 : 0;
        part = dup_cstr(raw);
        plen = strlen(part);
        if (previous_was_partial_stub && joined.n && plen) {
            size_t jlen = joined.n;
            size_t start = jlen >= plen ? jlen - plen : 0;
            size_t ov = continuation_overlap_length(joined.s + start, part);
            if (ov > plen) ov = plen;
            off = ov;
        }
        if (joined.n && !ascii_isspace((unsigned char)joined.s[joined.n - 1])
                && plen > off && !ascii_isspace((unsigned char)part[off]))
            sb_addc(&joined, '\n');
        sb_addn(&joined, part + off, plen - off);
        free(part);
        previous_was_partial_stub = stub; /* closing (final_response, False) handled by i==nparts */
        if (i == nparts) break;
        /* The final tuple is appended after the loop in Python; we appended it as i==nparts. */
    }
    if (!joined.s) return dup_cstr("");
    return joined.s;
}

/* ---- _absorb_continuation_fragments ---- */
char *w11_turnfinal_45ceb6_absorb_continuation_fragments(
    const char *final_response, const json_t *parts, json_t *messages, int *had_parts) {
    size_t i;
    if (!parts || parts->type != JSON_ARRAY || parts->c.count == 0) {
        if (had_parts) *had_parts = 0;
        return dup_cstr(final_response ? final_response : "");
    }
    if (had_parts) *had_parts = 1;
    if (messages && messages->type == JSON_ARRAY) {
        for (i = 0; i < messages->c.count; i++) {
            json_t *m = messages->c.items[i];
            if (m && m->type == JSON_OBJECT) {
                json_obj_del(m, "_length_continuation_fragment");
                json_obj_del(m, "_length_continuation_nudge");
            }
        }
    }
    return join_truncated_parts(parts, final_response);
}

/* ---- scaffolding pop loop from finish_text_response ---- */
static const char *const EPHEMERAL_FLAGS[] = {
    "_thinking_prefill", "_empty_recovery_synthetic", "_empty_terminal_sentinel",
    "_dropped_toolcall_nudge"
};
int w11_turnfinal_45ceb6_pop_ephemeral_scaffolding(json_t *messages) {
    int popped = 0;
    if (!messages || messages->type != JSON_ARRAY) return 0;
    while (messages->c.count > 0) {
        json_t *last = messages->c.items[messages->c.count - 1];
        int any = 0;
        size_t k;
        if (!last || last->type != JSON_OBJECT) break;
        for (k = 0; k < sizeof(EPHEMERAL_FLAGS) / sizeof(EPHEMERAL_FLAGS[0]); k++) {
            if (py_truthy(json_obj_get(last, EPHEMERAL_FLAGS[k]))) { any = 1; break; }
        }
        if (!any) break;
        {
            json_t *dead = last;
            json_array_remove(messages, messages->c.count - 1);
            json_free(dead);
        }
        popped++;
    }
    return popped;
}
