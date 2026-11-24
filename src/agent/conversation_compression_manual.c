/*
 * conversation_compression_manual.c - C home for agent/conversation_compression_manual.py.
 * The one sanctioned history mutation for /compress: flag parsing, the boundary-aware
 * split, the lock-skip / nothing-to-do verdicts, the verbatim-tail rejoin, the summary
 * payload and the surface-neutral render. Collaborators (token estimator, agent
 * _compress_context, compression_summary, the lock signal) are injected as decision
 * inputs; the partial-compress / feedback helpers are ported here as static mirrors.
 * PoP: w19_conversati_84b9cf_summary_committed @ agent/conversation_compression_manual.py:summary_committed
 * PoP: w19_conversati_84b9cf_parse_compress_args @ agent/conversation_compression_manual.py:parse_compress_args
 * PoP: w19_conversati_84b9cf_estimate_request_tokens @ agent/conversation_compression_manual.py:estimate_request_tokens
 * PoP: w19_conversati_84b9cf_compress_now @ agent/conversation_compression_manual.py:compress_now
 * PoP: w19_conversati_84b9cf_render_compress_result @ agent/conversation_compression_manual.py:render_compress_result
 */
#include "noc_w19_conversati_84b9cf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CC_MIN_MESSAGES 4
#define CC_DEFAULT_KEEP_LAST 2
#define CC_MAX_KEEP_LAST 100
#define CC_AGGRESSIVE_UNSUPPORTED \
    "--aggressive is not supported; use '/compress here [N]' to keep only recent exchanges, " \
    "or /undo to drop turns."

static char *cc_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static int cc_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* Python == for JSON trees (dict/list/scalar deep equality). */
static int cc_equal(const json_t *a, const json_t *b) {
    size_t i;
    if (!a || !b) return a == b;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING:
            return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (json_len(a) != json_len(b)) return 0;
            for (i = 0; i < json_len(a); i++)
                if (!cc_equal(json_get(a, i), json_get(b, i))) return 0;
            return 1;
        case JSON_OBJECT:
            if (json_object_size(a) != json_object_size(b)) return 0;
            for (i = 0; i < json_object_size(a); i++) {
                const char *k = json_object_get_key_at(a, i);
                if (!k || !cc_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
            }
            return 1;
        default:
            return 0;
    }
}

/* f"{n:,}" */
static char *cc_commas(long long n) {
    char raw[32], out[48];
    size_t i, len, at = 0, neg = 0, digits;
    snprintf(raw, sizeof(raw), "%lld", n);
    len = strlen(raw);
    if (len && raw[0] == '-') { neg = 1; len--; }
    digits = len;
    for (i = 0; i < digits; i++) {
        if (i && (digits - i) % 3 == 0) out[at++] = ',';
        out[at++] = raw[neg + i];
    }
    out[at] = '\0';
    if (neg) {
        memmove(out + 1, out, at + 1);
        out[0] = '-';
    }
    return cc_dup(out);
}

static const char *cc_role(const json_t *m) {
    if (!m || m->type != JSON_OBJECT) return NULL;
    return json_get_str(m, "role", NULL);
}

/* ---------------- summary_committed ---------------- */

json_t *w19_conversati_84b9cf_summary_committed(json_t *summary) {
    int committed;
    if (!cc_truthy(summary)) return json_bool(0);
    committed = !(cc_truthy(json_obj_get(summary, "noop"))
               || cc_truthy(json_obj_get(summary, "aborted"))
               || cc_truthy(json_obj_get(summary, "refused_would_grow")));
    return json_bool(committed);
}

/* ---------------- parse_compress_args ---------------- */

static long long cc_coerce_keep(const char *value) {
    char *end = NULL;
    long long n;
    if (!value) return CC_DEFAULT_KEEP_LAST;
    n = strtoll(value, &end, 10);
    if (!end || end == value) return CC_DEFAULT_KEEP_LAST;
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
    if (*end != '\0') return CC_DEFAULT_KEEP_LAST;
    if (n < 1) n = 1;
    if (n > CC_MAX_KEEP_LAST) n = CC_MAX_KEEP_LAST;
    return n;
}

static int cc_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.split(): whitespace runs, no empty tokens. Returns the token count. */
static size_t cc_split(const char *s, char *buf, size_t bufsz, char **tokens, size_t max_tokens) {
    size_t n = 0, at = 0, i = 0;
    if (!s) { if (bufsz) buf[0] = '\0'; return 0; }
    while (s[i]) {
        size_t start;
        while (s[i] && cc_is_space(s[i])) i++;
        if (!s[i]) break;
        start = at;
        while (s[i] && !cc_is_space(s[i])) {
            if (at + 1 < bufsz) buf[at++] = s[i];
            i++;
        }
        if (at + 1 < bufsz) buf[at++] = '\0';
        if (n < max_tokens) tokens[n] = buf + start;
        n++;
    }
    if (at < bufsz) buf[at] = '\0';
    return n;
}

static void cc_lower(char *s) {
    size_t i;
    if (!s) return;
    for (i = 0; s[i]; i++)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
}

json_t *w19_conversati_84b9cf_parse_compress_args(const char *raw_args) {
    char buf[8192];
    char *tokens[64];
    size_t ntokens, i;
    int preview = 0, aggressive = 0, partial = 0;
    long long keep_last = CC_DEFAULT_KEEP_LAST;
    char *focus = NULL, *rest, *text, *lowered;
    json_t *out = json_object();

    /* extract_compress_flags((raw_args or "").strip()) */
    {
        char *work = cc_dup(raw_args ? raw_args : "");
        char joined[8192];
        size_t at = 0, start = 0, end;
        if (!work) work = cc_dup("");
        while (work[start] && cc_is_space(work[start])) start++;
        end = strlen(work);
        while (end > start && cc_is_space(work[end - 1])) end--;
        memmove(work, work + start, end - start);
        work[end - start] = '\0';
        ntokens = cc_split(work, buf, sizeof(buf), tokens, 64);
        joined[0] = '\0';
        for (i = 0; i < ntokens; i++) {
            char low[512];
            size_t ln = strlen(tokens[i]);
            snprintf(low, sizeof(low), "%s", tokens[i]);
            cc_lower(low);
            if (strcmp(low, "--preview") == 0 || strcmp(low, "--dry-run") == 0
                || strcmp(low, "--dryrun") == 0) { preview = 1; continue; }
            if (strcmp(low, "--aggressive") == 0) { aggressive = 1; continue; }
            if (at && at + 1 < sizeof(joined)) joined[at++] = ' ';
            if (at + ln + 1 < sizeof(joined)) {
                memcpy(joined + at, tokens[i], ln);
                at += ln;
            }
            joined[at] = '\0';
        }
        rest = cc_dup(joined);
        free(work);
    }
    if (!rest) rest = cc_dup("");

    /* parse_partial_compress_args(rest) */
    {
        size_t start = 0, end;
        text = cc_dup(rest);
        free(rest);
        if (!text) text = cc_dup("");
        end = strlen(text);
        while (end > start && cc_is_space(text[end - 1])) end--;
        memmove(text, text + start, end - start);
        text[end - start] = '\0';
        lowered = cc_dup(text);
        if (!lowered) lowered = cc_dup("");
        cc_lower(lowered);
        if (!text[0]) {
            partial = 0;
            keep_last = CC_DEFAULT_KEEP_LAST;
            focus = NULL;
        } else {
            if (strncmp(lowered, "up to here", 10) == 0) {
                memmove(lowered, lowered + 6, strlen(lowered + 6) + 1);
                memmove(text, text + 6, strlen(text + 6) + 1);
            }
            {
                char tbuf[8192];
                char *tt[64];
                size_t n = cc_split(lowered, tbuf, sizeof(tbuf), tt, 64);
                const char *head = (n > 0) ? tt[0] : "";
                if (strcmp(head, "here") == 0) {
                    partial = 1;
                    keep_last = (n >= 2) ? cc_coerce_keep(tt[1]) : CC_DEFAULT_KEEP_LAST;
                    focus = NULL;
                } else if ((strcmp(head, "--keep") == 0 || strcmp(head, "-k") == 0) && n >= 2) {
                    partial = 1;
                    keep_last = cc_coerce_keep(tt[1]);
                    focus = NULL;
                } else if (strncmp(head, "--keep=", 7) == 0) {
                    partial = 1;
                    keep_last = cc_coerce_keep(head + 7);
                    focus = NULL;
                } else {
                    partial = 0;
                    keep_last = CC_DEFAULT_KEEP_LAST;
                    focus = text[0] ? cc_dup(text) : NULL;
                }
            }
        }
        free(text);
        free(lowered);
    }
    json_set(out, "preview", json_bool(preview));
    json_set(out, "aggressive", json_bool(aggressive));
    json_set(out, "partial", json_bool(partial));
    json_set(out, "keep_last", json_number((double)keep_last));
    json_set(out, "focus_topic", focus ? json_string(focus) : json_null());
    free(focus);
    return out;
}

/* ---------------- estimate_request_tokens ---------------- */

static double cc_estimate(json_t *messages, json_t *rough_map, double rough_default) {
    size_t n = (messages && messages->type == JSON_ARRAY) ? json_len(messages) : 0;
    char key[32];
    json_t *v;
    if (n == 0) return 0;
    snprintf(key, sizeof(key), "%zu", n);
    v = (rough_map && rough_map->type == JSON_OBJECT) ? json_obj_get(rough_map, key) : NULL;
    if (v && v->type == JSON_NUMBER) return v->num_val;
    return rough_default;
}

json_t *w19_conversati_84b9cf_estimate_request_tokens(json_t *messages, json_t *rough_map,
                                                      double rough_default) {
    return json_number(cc_estimate(messages, rough_map, rough_default));
}

/* ---------------- collaborators ported from the sibling modules ---------------- */

/* split_history_for_partial_compress(history, keep_last) -> (head, tail) */
static void cc_split_history(json_t *history, long long keep_last, json_t **out_head,
                             json_t **out_tail) {
    json_t *head = json_array();
    json_t *tail = json_array();
    size_t n = (history && history->type == JSON_ARRAY) ? json_len(history) : 0;
    long long boundary = -1, seen = 0;
    size_t i;
    if (keep_last < 1) keep_last = 1;
    if (n == 0) { *out_head = head; *out_tail = tail; return; }
    for (i = n; i-- > 0;) {
        const char *role = cc_role(json_get(history, i));
        if (role && strcmp(role, "user") == 0) {
            boundary = (long long)i;
            seen++;
            if (seen >= keep_last) break;
        }
    }
    if (boundary <= 0) {
        for (i = 0; i < n; i++) json_append(head, json_copy(json_get(history, i)));
    } else {
        for (i = 0; i < (size_t)boundary; i++) json_append(head, json_copy(json_get(history, i)));
        for (i = (size_t)boundary; i < n; i++) json_append(tail, json_copy(json_get(history, i)));
    }
    *out_head = head;
    *out_tail = tail;
}

/* rejoin_compressed_head_and_tail(head, tail) */
static json_t *cc_rejoin(json_t *head_in, json_t *tail) {
    json_t *head = json_array();
    size_t i, n = (tail && tail->type == JSON_ARRAY) ? json_len(tail) : 0;
    size_t rest_start = 0;
    if (!head_in || head_in->type != JSON_ARRAY) head_in = NULL;
    if (n == 0) {
        for (i = 0; head_in && i < json_len(head_in); i++)
            json_append(head, json_copy(json_get(head_in, i)));
        return head;
    }
    if (!head_in || json_len(head_in) == 0) {
        for (i = 0; i < n; i++) json_append(head, json_copy(json_get(tail, i)));
        return head;
    }
    for (i = 0; i < json_len(head_in); i++)
        json_append(head, json_copy(json_get(head_in, i)));
    {
        json_t *last = json_get(head, json_len(head) - 1);
        json_t *first = json_get(tail, 0);
        const char *last_role = cc_role(last), *first_role = cc_role(first);
        if (last_role && first_role && strcmp(last_role, first_role) == 0
            && (strcmp(last_role, "user") == 0 || strcmp(last_role, "assistant") == 0)) {
            json_t *lc = json_obj_get(last, "content");
            json_t *fc = json_obj_get(first, "content");
            if (lc && fc && lc->type == JSON_STRING && fc->type == JSON_STRING) {
                size_t ln = strlen(lc->str_val), fn = strlen(fc->str_val);
                char *joined = (char *)malloc(ln + fn + 3);
                if (joined) {
                    json_t *row = json_object();
                    size_t k;
                    for (k = 0; k < json_object_size(last); k++) {
                        const char *key = json_object_get_key_at(last, k);
                        if (key && strcmp(key, "content") != 0)
                            json_set(row, key, json_copy(json_object_get_at(last, k)));
                    }
                    memcpy(joined, lc->str_val, ln);
                    memcpy(joined + ln, "\n\n", 2);
                    memcpy(joined + ln + 2, fc->str_val, fn + 1);
                    json_set(row, "content", json_string(joined));
                    free(joined);
                    json_array_set(head, json_len(head) - 1, row);
                    rest_start = 1;
                }
            } else {
                json_t *bridge = json_object();
                json_set(bridge, "role",
                         json_string(strcmp(first_role, "user") == 0 ? "assistant" : "user"));
                json_set(bridge, "content", json_string(""));
                json_append(head, bridge);
            }
        }
    }
    for (i = rest_start; i < n; i++) json_append(head, json_copy(json_get(tail, i)));
    return head;
}

/* summarize_compress_preview(history, partial, keep_last, focus_topic, approx_tokens).lines */
static json_t *cc_preview_lines(json_t *history, int partial, long long keep_last,
                                const char *focus_topic, double approx_tokens) {
    json_t *lines = json_array();
    json_t *head = NULL, *tail = NULL;
    size_t total = (history && history->type == JSON_ARRAY) ? json_len(history) : 0;
    int effective_partial = 0;
    if (partial) {
        cc_split_history(history, keep_last, &head, &tail);
        effective_partial = json_len(tail) > 0;
    } else {
        size_t i;
        head = json_array();
        tail = json_array();
        for (i = 0; i < total; i++) json_append(head, json_copy(json_get(history, i)));
    }
    json_append(lines, json_string("Preview \342\200\224 no changes made."));
    {
        char *tokens_txt = cc_commas((long long)approx_tokens);
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Would compress %zu of %zu message(s) (~%s tokens currently in context).",
                 json_len(head), total, tokens_txt ? tokens_txt : "0");
        json_append(lines, json_string(buf));
        free(tokens_txt);
    }
    if (effective_partial) {
        char buf[192];
        char *num = cc_commas(keep_last);
        snprintf(buf, sizeof(buf),
                 "Boundary: keeping the last %s exchange(s) (%zu message(s)) verbatim.",
                 num ? num : "0", json_len(tail));
        free(num);
        json_append(lines, json_string(buf));
    } else if (partial) {
        json_append(lines, json_string(
            "Boundary: 'here' split would keep everything \342\200\224 falling back to full compression."));
    }
    if (focus_topic && focus_topic[0]) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "Focus topic: \"%s\"", focus_topic);
        json_append(lines, json_string(buf));
    }
    json_append(lines, json_string("Run the command again without --preview to apply."));
    json_free(head);
    json_free(tail);
    return lines;
}

/* summarize_manual_compression(...): headline / token_line / note. */
static json_t *cc_summary(json_t *before, json_t *after, double before_tokens, double after_tokens,
                          json_t *state) {
    json_t *out = json_object();
    size_t before_count = (before && before->type == JSON_ARRAY) ? json_len(before) : 0;
    size_t after_count = (after && after->type == JSON_ARRAY) ? json_len(after) : 0;
    int noop = cc_equal(before, after);
    json_t *ab = json_obj_get(state, "_last_compress_aborted");
    json_t *rf = json_obj_get(state, "_last_compress_refused_would_grow");
    json_t *fb = json_obj_get(state, "_last_summary_fallback_used");
    int aborted = ab && ab->type == JSON_BOOL && ab->bool_val;
    int refused = rf && rf->type == JSON_BOOL && rf->bool_val;
    int fallback = fb && fb->type == JSON_BOOL && fb->bool_val;
    const char *reason = json_get_str(state, "_last_summary_error", NULL);
    const char *note = NULL;
    char headline[512], token_line[256], fallback_note[256];
    char *bc = cc_commas((long long)before_count);
    char *ac = cc_commas((long long)after_count);
    char *bt = cc_commas((long long)before_tokens);
    char *at = cc_commas((long long)after_tokens);

    if (reason && !reason[0]) reason = NULL;

    if (refused) {
        snprintf(headline, sizeof(headline),
                 "Compression refused (summary would grow the conversation): %zu messages preserved",
                 before_count);
        note = "The generated summary was larger than what it would replace; no messages were removed.";
    } else if (aborted) {
        snprintf(headline, sizeof(headline), "Compression aborted: %zu messages preserved",
                 before_count);
        note = "Summary generation failed; no messages were removed.";
    } else if (fallback) {
        json_t *dc = json_obj_get(state, "_last_summary_dropped_count");
        long long dropped;
        if (dc && dc->type == JSON_NUMBER) dropped = (long long)dc->num_val;
        else dropped = (long long)before_count - (long long)after_count;
        if (dropped < 0) dropped = 0;
        snprintf(headline, sizeof(headline), "Compressed with fallback: %s \342\206\222 %s messages",
                 bc ? bc : "0", ac ? ac : "0");
        snprintf(fallback_note, sizeof(fallback_note),
                 "Summary generation failed; Hermes used limited fallback context "
                 "and removed %lld message(s).", dropped);
        note = fallback_note;
    } else if (noop) {
        snprintf(headline, sizeof(headline), "No changes from compression: %zu messages",
                 before_count);
    } else {
        snprintf(headline, sizeof(headline), "Compressed: %s \342\206\222 %s messages",
                 bc ? bc : "0", ac ? ac : "0");
        if (after_count < before_count && after_tokens > before_tokens)
            note = "Note: fewer messages can still raise this estimate when "
                   "compression rewrites the transcript into denser summaries.";
    }
    if ((noop && after_tokens == before_tokens) || refused) {
        snprintf(token_line, sizeof(token_line),
                 "Approx request size: ~%s tokens (unchanged)", bt ? bt : "0");
    } else {
        snprintf(token_line, sizeof(token_line),
                 "Approx request size: ~%s \342\206\222 ~%s tokens", bt ? bt : "0", at ? at : "0");
    }
    if (reason && (aborted || fallback)) {
        /* crosses a user-facing boundary: the live redactor passes plain text unchanged */
        static char with_reason[768];
        snprintf(with_reason, sizeof(with_reason), "%s Reason: %s", note ? note : "", reason);
        note = with_reason;
    }
    json_set(out, "noop", json_bool(noop));
    json_set(out, "aborted", json_bool(aborted));
    json_set(out, "refused_would_grow", json_bool(refused));
    json_set(out, "fallback_used", json_bool(fallback));
    json_set(out, "headline", json_string(headline));
    json_set(out, "token_line", json_string(token_line));
    json_set(out, "note", note ? json_string(note) : json_null());
    free(bc);
    free(ac);
    free(bt);
    free(at);
    return out;
}

/* describe_compression_lock_skip(lock_signal) */
static const char *cc_lock_skip_text(const char *holder, char *buf, size_t bufsz) {
    if (holder && holder[0]) {
        snprintf(buf, bufsz,
                 "\342\217\263 Compression already in progress for this session (holder: %s). "
                 "Please wait for it to finish.", holder);
        return buf;
    }
    snprintf(buf, bufsz,
             "\342\217\263 Compression skipped: could not acquire this session's compression lock. "
             "Another compression may still be running, or the lock check failed \342\200\224 try again shortly.");
    return buf;
}

/* ---------------- compress_now ---------------- */

static void cc_emit(json_t *out, const char *status, double before_tokens, double after_tokens,
                    long long removed, json_t *summary, json_t *lines, size_t after_count,
                    json_t *lock_holder) {
    json_t *committed;
    json_set(out, "status", json_string(status));
    json_set(out, "before_tokens", json_number(before_tokens));
    json_set(out, "after_tokens", json_number(after_tokens));
    json_set(out, "removed", json_number((double)removed));
    committed = w19_conversati_84b9cf_summary_committed(summary);
    json_set(out, "committed",
             json_bool(strcmp(status, "compressed") == 0 && cc_truthy(committed)));
    json_free(committed);
    json_set(out, "lock_holder", lock_holder ? lock_holder : json_null());
    json_set(out, "summary", summary ? summary : json_null());
    json_set(out, "lines", lines ? lines : json_array());
    json_set(out, "after_count", json_number((double)after_count));
}

json_t *w19_conversati_84b9cf_compress_now(json_t *spec) {
    json_t *out = json_object();
    json_t *history, *request, *rough_map, *compressed_in, *state, *summary = NULL;
    json_t *before, *after, *head, *tail;
    double before_tokens, after_tokens, rough_default;
    int preview, partial, skip, context_error, stamped;
    long long keep_last;
    const char *focus;
    json_t *lock_signal;

    if (!spec || spec->type != JSON_OBJECT) spec = json_object();
    history = json_obj_get(spec, "history");
    request = json_obj_get(spec, "request");
    rough_map = json_obj_get(spec, "rough_map");
    compressed_in = json_obj_get(spec, "compressed");
    state = json_obj_get(spec, "compression_state");
    if (!state || state->type != JSON_OBJECT) state = json_object();
    rough_default = json_get_num(spec, "rough_default", 0);

    preview = cc_truthy(json_obj_get(request, "preview"));
    partial = cc_truthy(json_obj_get(request, "partial"));
    focus = json_get_str(request, "focus_topic", NULL);
    if (focus && !focus[0]) focus = NULL;
    {
        json_t *kl = json_obj_get(request, "keep_last");
        keep_last = (kl && kl->type == JSON_NUMBER) ? (long long)kl->num_val : CC_DEFAULT_KEEP_LAST;
    }
    skip = cc_truthy(json_obj_get(spec, "skip_without_window"));
    context_error = cc_truthy(json_obj_get(spec, "context_error"));
    stamped = cc_truthy(json_obj_get(spec, "stamped"));

    before = json_array();
    if (history && history->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(history); i++)
            json_append(before, json_copy(json_get(history, i)));
    before_tokens = cc_estimate(before, rough_map, rough_default);

    head = json_array();
    tail = json_array();
    if (partial) {
        cc_split_history(before, keep_last, &head, &tail);
        if (json_len(tail) == 0) {
            json_free(head);
            head = json_array();
            for (size_t i = 0; i < json_len(before); i++)
                json_append(head, json_copy(json_get(before, i)));
        }
    } else {
        for (size_t i = 0; i < json_len(before); i++)
            json_append(head, json_copy(json_get(before, i)));
    }

    if (preview) {
        json_t *lines = cc_preview_lines(before, partial, keep_last, focus, before_tokens);
        cc_emit(out, "preview", before_tokens, before_tokens, 0, NULL, lines,
                json_len(before), NULL);
        json_free(before);
        json_free(head);
        json_free(tail);
        return out;
    }

    {
        json_t *has_content = json_obj_get(spec, "has_content");
        if (skip && has_content && has_content->type == JSON_BOOL && !has_content->bool_val) {
            cc_emit(out, "nothing_to_do", before_tokens, before_tokens, 0, NULL, NULL,
                    json_len(before), NULL);
            json_free(before);
            json_free(head);
            json_free(tail);
            return out;
        }
    }

    if (context_error) {
        /* agent._compress_context raised: the caller sees the exception, history untouched */
        json_set(out, "status", json_string("error"));
        json_free(before);
        json_free(head);
        json_free(tail);
        return out;
    }

    lock_signal = json_obj_get(spec, "lock_signal");
    if (lock_signal && ((lock_signal->type == JSON_BOOL && lock_signal->bool_val)
                        || lock_signal->type == JSON_STRING)) {
        json_t *holder = (lock_signal->type == JSON_STRING && lock_signal->str_val)
                       ? json_string(lock_signal->str_val) : NULL;
        cc_emit(out, "lock_skipped", before_tokens, before_tokens, 0, NULL, NULL,
                json_len(before), holder);
        json_free(before);
        json_free(head);
        json_free(tail);
        return out;
    }

    after = json_array();
    if (compressed_in && compressed_in->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(compressed_in); i++)
            json_append(after, json_copy(json_get(compressed_in, i)));
    if (json_len(tail) > 0 && !stamped) {
        json_t *joined = cc_rejoin(after, tail);
        json_free(after);
        after = joined;
    }
    after_tokens = cc_estimate(after, rough_map, rough_default);

    {
        json_t *agent_summary = json_obj_get(spec, "agent_summary");
        if (agent_summary && agent_summary->type == JSON_OBJECT)
            summary = json_copy(agent_summary);
        else
            summary = cc_summary(before, after, before_tokens, after_tokens, state);
    }

    cc_emit(out, "compressed", before_tokens, after_tokens,
            (long long)json_len(before) - (long long)json_len(after), summary, NULL,
            json_len(after), NULL);
    json_free(before);
    json_free(after);
    json_free(head);
    json_free(tail);
    return out;
}

json_t *w19_conversati_84b9cf_result_props(json_t *before_messages, json_t *after_messages,
                                           const char *status, json_t *summary) {
    json_t *out = json_object();
    size_t b = (before_messages && before_messages->type == JSON_ARRAY) ? json_len(before_messages) : 0;
    size_t a = (after_messages && after_messages->type == JSON_ARRAY) ? json_len(after_messages) : 0;
    json_t *committed = w19_conversati_84b9cf_summary_committed(summary);
    json_set(out, "removed", json_number((double)((long long)b - (long long)a)));
    json_set(out, "committed",
             json_bool(status && strcmp(status, "compressed") == 0 && cc_truthy(committed)));
    json_free(committed);
    return out;
}

json_t *w19_conversati_84b9cf_render_compress_result(json_t *result, const char *prefix) {
    json_t *lines = json_array();
    const char *status;
    char buf[2048];
    if (!prefix) prefix = "";
    if (!result || result->type != JSON_OBJECT) return lines;
    status = json_get_str(result, "status", "");
    if (!status) status = "";
    if (strcmp(status, "preview") == 0) {
        json_t *src = json_obj_get(result, "lines");
        for (size_t i = 0; src && i < json_len(src); i++) {
            json_t *v = json_get(src, i);
            snprintf(buf, sizeof(buf), "%s%s", prefix, (v && v->type == JSON_STRING) ? v->str_val : "");
            json_append(lines, json_string(buf));
        }
        return lines;
    }
    if (strcmp(status, "lock_skipped") == 0) {
        json_t *holder = json_obj_get(result, "lock_holder");
        char text[512];
        const char *h = (holder && holder->type == JSON_STRING) ? holder->str_val : NULL;
        cc_lock_skip_text(h, text, sizeof(text));
        snprintf(buf, sizeof(buf), "%s%s", prefix, text);
        json_append(lines, json_string(buf));
        return lines;
    }
    if (strcmp(status, "nothing_to_do") == 0) {
        snprintf(buf, sizeof(buf), "%sNothing to compress yet.", prefix);
        json_append(lines, json_string(buf));
        return lines;
    }
    {
        json_t *summary = json_obj_get(result, "summary");
        const char *keys[3] = { "headline", "token_line", "note" };
        size_t i;
        for (i = 0; i < 3; i++) {
            const char *v = json_get_str(summary, keys[i], NULL);
            if (v && v[0]) {
                snprintf(buf, sizeof(buf), "%s%s", prefix, v);
                json_append(lines, json_string(buf));
            }
        }
    }
    return lines;
}

json_t *w19_conversati_84b9cf_constants(void) {
    json_t *out = json_object();
    json_set(out, "MIN_MESSAGES", json_number(CC_MIN_MESSAGES));
    json_set(out, "AGGRESSIVE_UNSUPPORTED", json_string(CC_AGGRESSIVE_UNSUPPORTED));
    json_set(out, "DEFAULT_KEEP_LAST", json_number(CC_DEFAULT_KEEP_LAST));
    json_set(out, "MAX_KEEP_LAST", json_number(CC_MAX_KEEP_LAST));
    return out;
}
