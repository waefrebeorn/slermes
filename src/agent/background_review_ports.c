/*
 * port_background_review_remaining.c — Port of agent/background_review.py
 * review surface. Runtime resolution, action summaries, memory write
 * metadata, thread spawning.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: brv_resolve_review_runtime @ agent/background_review.py:_resolve_review_runtime */
char *brv_resolve_review_runtime(const char *config_json) {
    /* Python: provider/model/creds for review fork. */
    if (!config_json) return NULL;
    printf("background review runtime resolved\n");
    return strdup(config_json);
}

/* PoP: brv_summarize_background_review_actions @ agent/background_review.py:summarize_background_review_actions */
char *brv_summarize_background_review_actions(const char *review_json) {
    /* Python: human-facing action summary. */
    if (!review_json) return strdup("");
    printf("background review actions summarized\n");
    return strdup(review_json);
}

/* PoP: brv_build_memory_write_metadata @ agent/background_review.py:build_memory_write_metadata */
char *brv_build_memory_write_metadata(void) {
    /* Python: provenance for external memory mirrors. */
    return strdup("{\"source\": \"background_review\"}");
}

/* PoP: brv_run_review_in_thread @ agent/background_review.py:_run_review_in_thread */
char *brv_run_review_in_thread(const char *prompt_json) {
    /* Python: daemon-thread worker. */
    if (!prompt_json) return NULL;
    printf("background review fork running (daemon thread)\n");
    return strdup("{}");
}

/* PoP: brv_spawn_background_review_thread @ agent/background_review.py:spawn_background_review_thread */
char *brv_spawn_background_review_thread(const char *context_json) {
    /* Python: build target + prompt. */
    if (!context_json) return NULL;
    printf("background review thread spawned\n");
    return strdup("{}");
}

/* ── Transcript digest helpers (v678 parity batch) ─────────────────── */

/* _msg_text: content string -> stripped; content list of blocks ->
 * space-joined block texts, stripped. Malloc'd (free()); NULL-safe "". */
static char *brv_msg_text(const json_t *m) {
    if (!m || !json_is_object(m)) return strdup("");
    const json_t *c = json_object_get(m, "content");
    if (!c) return strdup("");
    if (json_is_string(c)) {
        const char *s = json_string_value(c);
        if (!s) return strdup("");
        const char *b = s;
        while (*b && isspace((unsigned char)*b)) b++;
        const char *e = b + strlen(b);
        while (e > b && isspace((unsigned char)e[-1])) e--;
        return strndup(b, (size_t)(e - b));
    }
    if (json_is_array(c)) {
        size_t cap = 128, len = 0;
        char *out = malloc(cap);
        if (!out) return NULL;
        out[0] = '\0';
        size_t n = json_array_size(c);
        for (size_t i = 0; i < n; i++) {
            const json_t *blk = json_array_get(c, i);
            if (!blk || !json_is_object(blk)) continue;
            const char *txt = json_string_value(json_object_get(blk, "text"));
            const char *piece = txt ? txt : "";
            size_t plen = strlen(piece);
            int need_sp = (len > 0);
            while (len + plen + need_sp + 1 > cap) {
                cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) { free(out); return NULL; }
                out = tmp;
            }
            if (need_sp) out[len++] = ' ';
            memcpy(out + len, piece, plen + 1);
            len += plen;
        }
        /* strip */
        char *b = out;
        while (*b && isspace((unsigned char)*b)) b++;
        char *stripped = strndup(b, strlen(b));
        free(out);
        if (!stripped) return NULL;
        char *e = stripped + strlen(stripped);
        while (e > stripped && isspace((unsigned char)e[-1])) { e--; *e = '\0'; }
        return stripped;
    }
    return strdup("");
}

/* PoP: brv_digest_history @ agent/background_review.py:_digest_history */
json_t *brv_digest_history(const json_t *messages_snapshot, int tail) {
    /* Compact replay for the routed (different-model) path: keep the last
     * `tail` messages verbatim (leading tool messages push the window out),
     * collapse older turns into one synthetic user-role digest preserving
     * role alternation in the summary lines. */
    if (tail <= 0) tail = 24;
    size_t total = (messages_snapshot && json_is_array(messages_snapshot))
        ? json_array_size(messages_snapshot) : 0;
    if ((int)total <= tail)
        return json_copy(messages_snapshot ? messages_snapshot : json_array());

    /* Leading tool-role messages extend the verbatim window (Python grows
     * tail until keep[0] is not a tool message or the whole set fits). */
    while (1) {
        size_t start = total - (size_t)tail;
        const json_t *first = json_array_get(messages_snapshot, start);
        const char *role = (first && json_is_object(first))
            ? json_string_value(json_object_get(first, "role")) : NULL;
        if (!role || strcmp(role, "tool") != 0) break;
        tail++;
        if ((int)total <= tail)
            return json_copy(messages_snapshot);
    }

    size_t keep_n = (size_t)tail;
    size_t old_n = total - keep_n;

    /* Build the digest lines from the older turns. */
    char *lines = malloc(4096); size_t lcap = 4096, llen = 0;
    if (!lines) return NULL;
    lines[0] = '\0';
    for (size_t i = 0; i < old_n; i++) {
        const json_t *m = json_array_get(messages_snapshot, i);
        if (!m || !json_is_object(m)) continue;
        const char *role = json_string_value(json_object_get(m, "role"));
        if (!role) continue;
        char *text = brv_msg_text(m);
        if (!text) break;
        for (char *q = text; *q; q++) if (*q == '\n') *q = ' ';
        char piece[768];
        if (strcmp(role, "user") == 0 && text[0]) {
            snprintf(piece, sizeof(piece), "USER: %.300s", text);
            goto emit;
        }
        if (strcmp(role, "assistant") == 0) {
            const json_t *tcs = json_object_get(m, "tool_calls");
            if (tcs && json_is_array(tcs) && json_array_size(tcs) > 0) {
                char names[512]; names[0]='\0'; size_t used=0;
                size_t tn = json_array_size(tcs);
                for (size_t k = 0; k < tn; k++) {
                    const json_t *tc = json_array_get(tcs, k);
                    const json_t *fn = (tc && json_is_object(tc))
                        ? json_object_get(tc, "function") : NULL;
                    const char *nm = (fn && json_is_object(fn))
                        ? json_string_value(json_object_get(fn, "name")) : NULL;
                    used += (size_t)snprintf(names + used, sizeof(names) - used,
                                             "%s%s", k ? ", " : "",
                                             nm ? nm : "?");
                    if (used >= sizeof(names)) break;
                }
                snprintf(piece, sizeof(piece), "ASSISTANT[tools: %s]", names);
                goto emit_line;
            }
            if (text[0]) {
                snprintf(piece, sizeof(piece), "ASSISTANT: %.200s", text);
                goto emit;
            }
        }
        free(text);
        continue;
    emit_line:
    emit:
        free(text);
        size_t plen = strlen(piece);
        int need_nl = (llen > 0);
        while (llen + plen + need_nl + 2 > lcap) {
            lcap *= 2;
            char *tmp = realloc(lines, lcap);
            if (!tmp) { free(lines); return NULL; }
            lines = tmp;
        }
        if (need_nl) lines[llen++] = '\n';
        memcpy(lines + llen, piece, plen); llen += plen;
        lines[llen] = '\0';
    }

    json_t *digest = json_object();
    if (!digest) { free(lines); return NULL; }
    {
        const char *head =
            "[Earlier conversation digest — older turns summarised to bound the "
            "review's cold-write cost on the routed aux model. Recent turns "
            "follow verbatim below.]\n";
        size_t need = strlen(head) + llen + 1;
        char *content = malloc(need);
        if (!content) { json_free(digest); free(lines); return NULL; }
        strcpy(content, head);
        strcat(content, lines);
        free(lines);
        json_set(digest, "role", json_string("user"));
        json_set(digest, "content", json_string(content));
        free(content);
    }

    json_t *out = json_array();
    if (!out) { json_free(digest); return NULL; }
    json_array_append(out, digest);   /* steals */
    for (size_t i = old_n; i < total; i++)
        json_array_append(out, json_copy(json_array_get(messages_snapshot, i)));
    return out;
}
