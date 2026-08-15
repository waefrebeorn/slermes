/* Slermes C port — small-module DA-4 gaps (pure functions) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"
#include "hermes_logger.h"
#include "libcrypto/crypto.h"

/* ── agent/retry_utils.py:parse_retry_after_seconds ── */
/* PoP: retry_parse_retry_after_seconds @ agent/retry_utils.py:parse_retry_after_seconds */
double retry_parse_retry_after_seconds(const char *raw, bool *ok) {
    if (!raw || !*raw) { if (ok) *ok = false; return 0.0; }
    char *end = NULL;
    double val = strtod(raw, &end);
    if (end && *end == '\0') {
        if (ok) *ok = true;
        return val > 0.0 ? val : 0.0;
    }
    /* TODO: HTTP-date form (RFC 7231) — parse with libdatetime */
    if (ok) *ok = false;
    return 0.0;
}

/* ── gateway/slash_commands.py:_redact_matrix_session_key ── */
/* PoP: slash_redact_matrix_session_key @ gateway/slash_commands.py:_redact_matrix_session_key */
void slash_redact_matrix_session_key(const char *session_key, char *out, size_t out_sz) {
    if (!session_key) session_key = "";
    unsigned char hash[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)session_key, strlen(session_key), hash);
    snprintf(out, out_sz, "sha256:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
             hash[0], hash[1], hash[2], hash[3], hash[4], hash[5],
             hash[6], hash[7], hash[8], hash[9], hash[10], hash[11]);
}

/* ── agent/turn_summary.py:_ordered_verbs ── */
/* PoP: ts_ordered_verbs @ agent/turn_summary.py:_ordered_verbs */
json_t *ts_ordered_verbs(json_t *verbs_tally) {
    json_t *result = json_array();
    if (!verbs_tally || verbs_tally->type != JSON_OBJECT) return result;
    const char *priority[] = {"tool", "file", "web", "thinking", NULL};
    for (int i = 0; priority[i]; i++) {
        if (json_obj_get(verbs_tally, priority[i])) {
            json_append(result, json_string(priority[i]));
        }
    }
    /* Append remaining keys (first-seen order) */
    size_t n = json_len(verbs_tally);
    for (size_t idx = 0; idx < n; idx++) {
        const char *key = verbs_tally->c.keys[idx];
        bool already = false;
        for (int i = 0; priority[i]; i++) {
            if (strcmp(key, priority[i]) == 0) { already = true; break; }
        }
        if (!already) {
            json_append(result, json_string(key));
        }
    }
    return result;
}

/* ── agent/credits_tracker.py:new_credits_latch ── */
/* PoP: credits_new_latch @ agent/credits_tracker.py:new_credits_latch */
json_t *credits_new_latch(void) {
    json_t *latch = json_object();
    json_set(latch, "active", json_array());
    json_set(latch, "seen_below_90", json_bool(false));
    json_set(latch, "usage_band", json_null());
    json_set(latch, "seen_grant_unspent", json_bool(false));
    return latch;
}

/* ── agent/learning_graph_render.py:build_summary ── */
/* PoP: learn_graph_build_summary @ agent/learning_graph_render.py:build_summary */
json_t *learn_graph_build_summary(json_t *payload) {
    if (!payload) return json_array();
    json_t *stats = json_obj_get(payload, "stats");
    if (!stats || stats->type != JSON_OBJECT) stats = json_object();
    json_t *lines = json_array();

    int learned = (int)json_get_num(stats, "learned_skills", (int)json_get_num(stats, "nodes", 0));
    int mem = (int)json_get_num(stats, "memory_nodes", 0);
    int edges = (int)json_get_num(stats, "related_edges", 0);

    char buf[256];
    snprintf(buf, sizeof(buf), "%d learned skills · %d memories · %d skill links", learned, mem, edges);
    json_append(lines, json_string(buf));

    return lines;
}
