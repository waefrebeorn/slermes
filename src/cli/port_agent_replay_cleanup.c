/*
 * port_agent_replay_cleanup.c — C port of agent/replay_cleanup.py
 *
 * Pure replay-history sanitization helpers. No config, no network, no async.
 * They operate on a plain "agent history" representation modelled here as a
 * dynamic array of message structs (role + content), which is exactly what
 * the Python list[dict] carries. Faithful to is_interrupted_tool_result,
 * strip_interrupted_tool_tails, strip_dangling_tool_call_tail, and
 * sanitize_replay_history.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

typedef struct {
    char role[16];      /* "assistant" | "tool" | "user" | ... */
    char *content;      /* owned, may be NULL */
} replay_msg_t;

typedef struct {
    replay_msg_t *msgs;
    size_t count;
    size_t cap;
} replay_hist_t;

static void replay_init(replay_hist_t *h) { h->msgs = NULL; h->count = 0; h->cap = 0; }
static void replay_free(replay_hist_t *h) {
    for (size_t i = 0; i < h->count; i++) free(h->msgs[i].content);
    free(h->msgs);
    h->msgs = NULL; h->count = 0; h->cap = 0;
}
static int replay_push(replay_hist_t *h, const char *role, const char *content) {
    if (h->count == h->cap) {
        size_t nc = h->cap ? h->cap * 2 : 8;
        replay_msg_t *nm = realloc(h->msgs, nc * sizeof(*nm));
        if (!nm) return -1;
        h->msgs = nm; h->cap = nc;
    }
    replay_msg_t *m = &h->msgs[h->count++];
    snprintf(m->role, sizeof(m->role), "%s", role);
    m->content = content ? strdup(content) : NULL;
    return 0;
}

/* ---- lowercase copy ---- */
static void lc_buf(const char *s, char *out, size_t outsz) {
    size_t i = 0;
    for (; s && s[i] && i + 1 < outsz; i++)
        out[i] = (char)tolower((unsigned char)s[i]);
    out[i] = '\0';
}

/* PoP: replay_cleanup_is_interrupted @ agent/replay_cleanup.py:is_interrupted_tool_result */
/* True if a tool result (string) indicates the tool was interrupted. */
bool replay_cleanup_is_interrupted(const char *content) {
    if (!content) return false;
    char low[8192];
    lc_buf(content, low, sizeof(low));
    if (strstr(low, "[command interrupted]")) return true;
    /* exit_code present AND (130 or -1) present AND "interrupt" present ->
     * Python: "exit_code" in lowered and ("130" in lowered or "-1" in lowered)
     *         and "interrupt" in lowered */
    if (strstr(low, "exit_code") &&
        (strstr(low, "130") || strstr(low, "-1")) &&
        strstr(low, "interrupt")) {
        return true;
    }
    return false;
}

/* PoP: replay_cleanup_strip_tails @ agent/replay_cleanup.py:strip_interrupted_tool_tails */
/* Strip interrupted assistant->tool blocks from replay history (in place).
 * Returns the new count (<= original). */
size_t replay_cleanup_strip_tails(replay_hist_t *h) {
    if (!h || h->count == 0) return h ? h->count : 0;
    replay_hist_t cleaned;
    replay_init(&cleaned);
    size_t i = 0, n = h->count;
    while (i < n) {
        replay_msg_t *m = &h->msgs[i];
        if (strcmp(m->role, "assistant") == 0 && m->content && strstr(m->content, "tool_calls")) {
            /* collect following contiguous tool results */
            size_t j = i + 1;
            int has_interrupted = 0;
            while (j < n && strcmp(h->msgs[j].role, "tool") == 0) {
                if (replay_cleanup_is_interrupted(h->msgs[j].content))
                    has_interrupted = 1;
                j++;
            }
            if (has_interrupted) {
                /* drop this assistant + its tool results */
                i = j;
                continue;
            }
        }
        if (strcmp(m->role, "tool") == 0 && replay_cleanup_is_interrupted(m->content)) {
            /* orphan interrupted tool result */
            i++;
            continue;
        }
        replay_push(&cleaned, m->role, m->content);
        i++;
    }
    /* swap cleaned into h */
    replay_free(h);
    *h = cleaned;
    return h->count;
}

/* PoP: replay_cleanup_strip_dangling @ agent/replay_cleanup.py:strip_dangling_tool_call_tail */
/* Strip a trailing assistant(tool_calls) block with NO tool answers. */
size_t replay_cleanup_strip_dangling(replay_hist_t *h) {
    if (!h || h->count == 0) return h ? h->count : 0;
    replay_msg_t *last = &h->msgs[h->count - 1];
    if (strcmp(last->role, "assistant") == 0 &&
        last->content && strstr(last->content, "tool_calls")) {
        /* confirmed: drop the trailing dangling tail */
        free(last->content);
        h->count--;
    }
    return h->count;
}

/* PoP: replay_cleanup_sanitize @ agent/replay_cleanup.py:sanitize_replay_history */
/* Apply both strippers in canonical order. Returns new count. */
size_t replay_cleanup_sanitize(replay_hist_t *h) {
    if (!h || h->count == 0) return h ? h->count : 0;
    replay_cleanup_strip_tails(h);
    replay_cleanup_strip_dangling(h);
    return h->count;
}
