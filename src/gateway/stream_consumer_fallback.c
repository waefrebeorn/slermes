/*
 * stream_consumer_fallback.c - C home for gateway/stream_consumer_fallback.py.
 * Faithful port of the fallback-delivery string logic: what the user has already seen,
 * the unseen tail (fence-reopening and prose resume included), and the retry/uncertainty
 * predicates that decide whether a failed send may be repeated.
 * PoP: w19_streamcons_39b4a2_edit_skipped @ gateway/stream_consumer_fallback.py:_edit_skipped
 * PoP: w19_streamcons_39b4a2_visible_prefix @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._visible_prefix
 * PoP: w19_streamcons_39b4a2_continuation_text @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._continuation_text
 * PoP: w19_streamcons_39b4a2_prose_resume_point @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._prose_resume_point
 * PoP: w19_streamcons_39b4a2_fragment_can_be_replaced @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._fragment_can_be_replaced
 * PoP: w19_streamcons_39b4a2_send_failure_may_have_delivered @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._send_failure_may_have_delivered
 * PoP: w19_streamcons_39b4a2_is_flood_error @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._is_flood_error
 * PoP: w19_streamcons_39b4a2_flood_retry_delay @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._fallback_flood_retry_delay
 * PoP: w19_streamcons_39b4a2_adopt_partial_head @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._adopt_partial_head
 * PoP: w19_streamcons_39b4a2_raw_message_limit @ gateway/stream_consumer_fallback.py:StreamFallbackMixin._raw_message_limit
 * PoP: w19_streamcons_39b4a2_open_code_fence_span @ gateway/stream_consumer_fences.py:open_code_fence_span
 * PoP: w19_streamcons_39b4a2_ensure_closed_code_fences @ gateway/stream_consumer_fences.py:ensure_closed_code_fences
 */
#include "noc_w19_streamcons_39b4a2.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FENCE "```"

static int py_isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *dup_range(const char *s, size_t a, size_t b) {
    char *out;
    if (b < a) b = a;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *dup_str(const char *s) {
    return s ? dup_range(s, 0, strlen(s)) : dup_range("", 0, 0);
}

/* str.count("```") - non-overlapping occurrences. */
static int count_fence(const char *s, size_t upto) {
    size_t i = 0, n;
    int c = 0;
    if (!s) return 0;
    n = strlen(s);
    if (upto > n) upto = n;
    while (i + 3 <= upto) {
        if (!strncmp(s + i, FENCE, 3)) {
            c++;
            i += 3;
        } else {
            i++;
        }
    }
    return c;
}

/* str.rfind("```", 0, end) - last index strictly before end, -1 when none. */
static long rfind_fence(const char *s, size_t end) {
    size_t n = s ? strlen(s) : 0;
    long i;
    if (end > n) end = n;
    for (i = (long)end - 1; i >= 0; i--)
        if ((size_t)i + 3 <= end && !strncmp(s + i, FENCE, 3)) return i;
    return -1;
}

static long find_char(const char *s, char c, size_t from) {
    size_t n = s ? strlen(s) : 0;
    for (; from < n; from++)
        if (s[from] == c) return (long)from;
    return -1;
}

static int ends_with(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return b <= a && !strcmp(s + a - b, suffix);
}

static char *lstrip_dup(const char *s) {
    size_t i = 0;
    while (s && s[i] && py_isspace((unsigned char)s[i])) i++;
    return dup_range(s ? s : "", i, s ? strlen(s) : 0);
}

static size_t leading_space(const char *s) {
    size_t i = 0;
    while (s && s[i] && py_isspace((unsigned char)s[i])) i++;
    return i;
}

/* Copy with complete ```...``` regions removed, then cut at a trailing ``` that no
 * backtick follows - the two re.sub passes the backtick count is taken over. */
static char *strip_fenced(const char *text) {
    char *work = dup_str(text);
    char *out;
    size_t i = 0, w = 0, len;
    if (!work) return NULL;
    len = strlen(work);
    out = (char *)malloc(len + 1);
    if (!out) return work;
    while (i < len) {
        if (i + 3 <= len && !strncmp(work + i, FENCE, 3)) {
            size_t j = i + 3;
            int hit = -1;
            while (j + 3 <= len) {
                if (!strncmp(work + j, FENCE, 3)) { hit = (int)j; break; }
                j++;
            }
            if (hit >= 0) {
                i = (size_t)hit + 3;
                continue;
            }
        }
        out[w++] = work[i++];
    }
    out[w] = '\0';
    free(work);
    work = out;
    len = strlen(work);
    for (i = 0; i + 3 <= len; i++) {
        if (!strncmp(work + i, FENCE, 3)) {
            size_t k = i + 3;
            int has_backtick = 0;
            for (; k < len; k++) if (work[k] == '`') { has_backtick = 1; break; }
            if (!has_backtick) { work[i] = '\0'; break; }
        }
    }
    return work;
}

/* ensure_closed_code_fences: close an orphaned fence, then an orphaned backtick. */
static char *ensure_closed(const char *text) {
    char *work = dup_str(text);
    char *without;
    size_t len, i, bt = 0;
    if (!work || !work[0]) return work;
    len = strlen(work);
    if (count_fence(work, len) % 2 == 1) {
        size_t m = len;
        char *grown;
        while (m > 0 && work[m - 1] == '\n') m--;
        grown = (char *)malloc(m + 5);
        if (!grown) return work;
        memcpy(grown, work, m);
        memcpy(grown + m, "\n```", 4);
        grown[m + 4] = '\0';
        free(work);
        work = grown;
    }
    without = strip_fenced(work);
    for (i = 0; without && without[i]; i++) if (without[i] == '`') bt++;
    free(without);
    if (bt % 2 == 1) {
        len = strlen(work);
        {
            char *grown = (char *)malloc(len + 2);
            if (grown) {
                memcpy(grown, work, len);
                grown[len] = '`';
                grown[len + 1] = '\0';
                free(work);
                work = grown;
            }
        }
    }
    return work;
}

json_t *w19_streamcons_39b4a2_ensure_closed_code_fences(const char *text) {
    char *s = ensure_closed(text ? text : "");
    json_t *out = json_string(s);
    free(s);
    return out;
}

json_t *w19_streamcons_39b4a2_open_code_fence_span(const char *text, long upto) {
    const char *t = text ? text : "";
    size_t n = strlen(t);
    long start, end;
    json_t *out;
    if (upto < 0) upto = 0;
    if ((size_t)upto > n) upto = (long)n;
    if (count_fence(t, (size_t)upto) % 2 == 0) return json_null();
    start = rfind_fence(t, (size_t)upto);
    if (start < 0) return json_null();
    end = find_char(t, '\n', (size_t)start);
    if (end < 0) end = (long)n;
    out = json_object();
    {
        json_t *span = json_array();
        json_append(span, json_number((double)start));
        json_append(span, json_number((double)end));
        json_set(out, "span", span);
    }
    return out;
}

/* _edit_skipped: raw_response is a dict and raw_response["skipped"] is truthy. */
json_t *w19_streamcons_39b4a2_edit_skipped(json_t *result) {
    json_t *raw = result && result->type == JSON_OBJECT ? json_obj_get(result, "raw_response") : NULL;
    int skipped = 0;
    if (raw && raw->type == JSON_OBJECT) {
        json_t *v = json_obj_get(raw, "skipped");
        if (v && v->type == JSON_BOOL) skipped = v->bool_val ? 1 : 0;
        else if (v && v->type == JSON_NUMBER) skipped = v->num_val != 0.0 ? 1 : 0;
        else if (v && v->type == JSON_STRING) skipped = v->str_val && v->str_val[0] ? 1 : 0;
        else if (v && v->type == JSON_ARRAY) skipped = json_len(v) > 0 ? 1 : 0;
        else if (v && v->type == JSON_OBJECT) skipped = json_object_size(v) > 0 ? 1 : 0;
    }
    return json_bool(skipped ? true : false);
}

/* _visible_prefix: last_sent_text with the cursor decoration removed. */
static char *visible_prefix_alloc(const char *last_sent_text, const char *cursor) {
    char *prefix = dup_str(last_sent_text ? last_sent_text : "");
    size_t curn = cursor ? strlen(cursor) : 0;
    if (curn && ends_with(prefix, cursor)) {
        size_t pn = strlen(prefix);
        if (curn <= pn) prefix[pn - curn] = '\0';
    } else if (curn && strstr(prefix, cursor)) {
        long at = -1;
        {
            const char *p = prefix;
            while ((p = strstr(p, cursor)) != NULL) { at = (long)(p - prefix); p++; }
        }
        if (at >= 0) {
            char *raw = dup_range(prefix, 0, (size_t)at);
            char *joined = (char *)malloc(strlen(raw) + curn + 1);
            char *reclosed;
            if (joined) {
                memcpy(joined, raw, strlen(raw));
                memcpy(joined + strlen(raw), cursor, curn + 1);
                reclosed = ensure_closed(joined);
                if (!strcmp(reclosed, prefix)) {
                    char *raw_closed = ensure_closed(raw);
                    free(prefix);
                    prefix = raw_closed;
                    free(reclosed);
                } else {
                    free(reclosed);
                }
                free(joined);
            }
            free(raw);
        }
    }
    return prefix;
}

json_t *w19_streamcons_39b4a2_visible_prefix(const char *last_sent_text, const char *cursor) {
    char *s = visible_prefix_alloc(last_sent_text, cursor);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

/* _prose_resume_point: back a cut up to the last space or newline. */
static long prose_resume(const char *text, long cut) {
    size_t n = strlen(text);
    long boundary, space_at, nl_at, i;
    if (cut < 0) return cut;
    if ((size_t)cut < n) {
        char cur = text[cut];
        /* final_text[cut - 1] wraps to the last character at cut == 0, as Python does */
        char prev = cut > 0 ? text[cut - 1] : (n ? text[n - 1] : '\0');
        if (!py_isspace((unsigned char)cur) && !py_isspace((unsigned char)prev)) {
            space_at = -1;
            for (i = 0; i < cut; i++) if (text[i] == ' ') space_at = i;
            nl_at = -1;
            for (i = 0; i < cut; i++) if (text[i] == '\n') nl_at = i;
            boundary = space_at > nl_at ? space_at : nl_at;
            if (boundary >= 0) return boundary + 1;
            for (i = cut; (size_t)i < n; i++)
                if (py_isspace((unsigned char)text[i])) return 0;
        }
    }
    return cut;
}

json_t *w19_streamcons_39b4a2_prose_resume_point(const char *final_text, long cut) {
    return json_number((double)prose_resume(final_text ? final_text : "", cut));
}

/* _fragment_can_be_replaced: an id, no preserved partial, and an adapter that deletes. */
json_t *w19_streamcons_39b4a2_fragment_can_be_replaced(const char *message_id, int preserve_partial,
                                                       int adapter_can_delete) {
    int ok = 1;
    if (!message_id || !strcmp(message_id, "__no_edit__")) ok = 0;
    if (preserve_partial) ok = 0;
    if (!adapter_can_delete) ok = 0;
    return json_bool(ok ? true : false);
}

/* _send_failure_may_have_delivered: a retry could duplicate something already on screen. */
json_t *w19_streamcons_39b4a2_send_failure_may_have_delivered(int retryable, int delivery_uncertain,
                                                             const char *error, const char *exc_name) {
    char *err_lower, *name_lower;
    int shown;
    if (retryable) return json_bool(false);
    if (delivery_uncertain) return json_bool(true);
    err_lower = dup_str(error ? error : "");
    name_lower = dup_str(exc_name ? exc_name : "");
    for (char *p = err_lower; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    for (char *p = name_lower; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    shown = strstr(err_lower, "timeout") != NULL || strstr(err_lower, "timed out") != NULL ||
            strstr(name_lower, "timeout") != NULL;
    free(err_lower);
    free(name_lower);
    return json_bool(shown ? true : false);
}

/* _is_flood_error: flood control / rate limiting in the error text. */
static int flood_error(const char *error) {
    char *e = dup_str(error ? error : "");
    int hit;
    for (char *p = e; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    hit = strstr(e, "flood") != NULL || strstr(e, "retry after") != NULL || strstr(e, "rate") != NULL;
    free(e);
    return hit;
}

json_t *w19_streamcons_39b4a2_is_flood_error(const char *error) {
    return json_bool(flood_error(error) ? true : false);
}

/* _fallback_flood_retry_delay: a bounded delay, or None when a retry is not safe. */
json_t *w19_streamcons_39b4a2_flood_retry_delay(const char *error, int has_retry_after,
                                                double retry_after, double max_wait) {
    double delay;
    if (!flood_error(error)) return json_null();
    delay = (has_retry_after && retry_after != 0.0) ? retry_after : 3.0;
    if (delay > max_wait) return json_null();
    if (delay < 0.0) delay = 0.0;
    return json_number(delay);
}

/* _adopt_partial_head: keep the parts that landed as the shown prefix. */
json_t *w19_streamcons_39b4a2_adopt_partial_head(json_t *result, const char *text) {
    json_t *out = json_object();
    json_t *raw = result && result->type == JSON_OBJECT ? json_obj_get(result, "raw_response") : NULL;
    json_t *prefix = NULL;
    const char *text_s = text ? text : "";
    const char *msg_id = NULL;
    int adopted = 0;
    if (raw && raw->type == JSON_OBJECT) {
        json_t *overflow = json_obj_get(raw, "partial_overflow");
        int has_overflow = 0;
        if (overflow && overflow->type == JSON_BOOL) has_overflow = overflow->bool_val ? 1 : 0;
        else if (overflow && overflow->type == JSON_NUMBER) has_overflow = overflow->num_val != 0.0 ? 1 : 0;
        else if (overflow && overflow->type == JSON_STRING) has_overflow = overflow->str_val && overflow->str_val[0] ? 1 : 0;
        if (has_overflow) prefix = json_obj_get(raw, "delivered_prefix");
    }
    if (prefix && prefix->type == JSON_STRING && prefix->str_val) {
        size_t lead = leading_space(prefix->str_val);
        size_t pn = strlen(prefix->str_val);
        int prefix_nonblank = lead < pn;
        int starts = !strncmp(text_s, prefix->str_val, pn);
        if (prefix_nonblank && starts) {
            json_t *last_id = raw ? json_obj_get(raw, "last_message_id") : NULL;
            json_t *result_id = result ? json_obj_get(result, "message_id") : NULL;
            adopted = 1;
            if (last_id && last_id->type == JSON_STRING && last_id->str_val[0]) msg_id = last_id->str_val;
            else if (result_id && result_id->type == JSON_STRING && result_id->str_val[0]) msg_id = result_id->str_val;
            else msg_id = "__no_edit__";
        }
    }
    json_set(out, "adopted", json_bool(adopted ? true : false));
    json_set(out, "message_id", msg_id ? json_string(msg_id) : json_null());
    json_set(out, "last_sent_text", json_string(adopted ? prefix->str_val : ""));
    json_set(out, "preserve_partial", json_bool(adopted ? true : false));
    json_set(out, "prefix", json_string(adopted ? prefix->str_val : ""));
    return out;
}

/* _continuation_text: only the part of final_text the user has not already seen. */
json_t *w19_streamcons_39b4a2_continuation_text(const char *final_text, const char *fallback_prefix,
                                               const char *last_sent_text, const char *cursor,
                                               int fragment_can_be_replaced) {
    const char *ft = final_text ? final_text : "";
    size_t fn = strlen(ft);
    size_t lead = leading_space(ft);
    char *prefix = NULL;
    json_t *out = json_object();
    json_t *fragment_cut = json_null();

    if (fallback_prefix && fallback_prefix[0]) {
        json_t *vp = w19_streamcons_39b4a2_visible_prefix(last_sent_text, cursor);
        prefix = lstrip_dup(fallback_prefix);
        json_free(vp);
    } else {
        char *vp = visible_prefix_alloc(last_sent_text, cursor);
        prefix = lstrip_dup(vp);
        free(vp);
    }
    if (prefix && prefix[0]) {
        size_t pn = strlen(prefix);
        if (lead <= fn && pn <= fn - lead && !strncmp(ft + lead, prefix, pn)) {
            size_t cut = lead + pn;
            json_t *span = w19_streamcons_39b4a2_open_code_fence_span(ft, (long)cut);
            if (!span || span->type != JSON_OBJECT) {
                long resume = prose_resume(ft, (long)cut);
                json_free(span);
                if (resume <= (long)lead) {
                    if (fragment_can_be_replaced) {
                        fragment_cut = json_number((double)cut);
                        json_set(out, "text", json_string(ft));
                        json_set(out, "fragment_cut", fragment_cut);
                        free(prefix);
                        return out;
                    }
                    resume = (long)cut;
                }
                {
                    char *tail = dup_range(ft, (size_t)resume, fn);
                    char *ls = lstrip_dup(tail);
                    json_set(out, "text", json_string(ls));
                    free(ls);
                    free(tail);
                }
                json_set(out, "fragment_cut", json_null());
                free(prefix);
                return out;
            } else {
                json_t *arr = json_obj_get(span, "span");
                long start = arr && json_len(arr) > 0 ? (long)json_get(arr, 0)->num_val : 0;
                long end = arr && json_len(arr) > 1 ? (long)json_get(arr, 1)->num_val : 0;
                char *tail;
                char *joined;
                json_free(span);
                if ((long)cut > end) {
                    char cur = (size_t)cut < fn ? ft[cut] : '\n';
                    char nxt = (size_t)cut < fn ? ((size_t)cut + 1 < fn ? ft[cut + 1] : '\n') : '\n';
                    char prev = ft[cut - 1];
                    if (prev != '\n' && cur != '\n' && nxt != '\n') {
                        long nl = -1;
                        for (long i = 0; i < (long)cut; i++) if (ft[i] == '\n') nl = i;
                        cut = (size_t)(nl + 1);
                    }
                }
                {
                    size_t from = (size_t)cut > (size_t)end ? (size_t)cut : (size_t)end;
                    char *raw_tail = dup_range(ft, from, fn);
                    size_t k = 0;
                    while (raw_tail[k] == '\n') k++;
                    tail = dup_range(raw_tail, k, strlen(raw_tail));
                    free(raw_tail);
                }
                {
                    /* tail.replace("```", "").strip() decides whether anything follows */
                    char *stripped = (char *)malloc(strlen(tail) + 1);
                    size_t r = 0;
                    const char *p = tail;
                    while (*p) {
                        if (!strncmp(p, FENCE, 3)) { p += 3; continue; }
                        stripped[r++] = *p++;
                    }
                    stripped[r] = '\0';
                    {
                        size_t a = 0, b = strlen(stripped);
                        while (a < b && py_isspace((unsigned char)stripped[a])) a++;
                        while (b > a && py_isspace((unsigned char)stripped[b - 1])) b--;
                        if (b > a) {
                            char *head = dup_range(ft, (size_t)start, (size_t)end);
                            size_t hl = strlen(head);
                            joined = (char *)malloc(hl + 1 + strlen(tail) + 1);
                            if (joined) {
                                memcpy(joined, head, hl);
                                joined[hl] = '\n';
                                memcpy(joined + hl + 1, tail, strlen(tail) + 1);
                                json_set(out, "text", json_string(joined));
                                free(joined);
                            }
                            free(head);
                        } else {
                            json_set(out, "text", json_string(""));
                        }
                    }
                    free(stripped);
                }
                json_set(out, "fragment_cut", json_null());
                free(tail);
                free(prefix);
                return out;
            }
        }
    }
    json_set(out, "text", json_string(ft));
    json_set(out, "fragment_cut", json_null());
    free(prefix);
    return out;
}

/* _raw_message_limit: the per-chat budget, raised by streaming_overflow_limit when larger. */
json_t *w19_streamcons_39b4a2_raw_message_limit(int is_base, int has_max, double max_value,
                                                int chat_limit_raises, int has_chat_limit,
                                                double chat_limit, int cap_kind, double cap_value) {
    double base = has_max ? max_value : 4096.0;
    if (is_base) {
        if (has_chat_limit && !chat_limit_raises) base = chat_limit;
        /* cap_kind: 0 absent, 1 int, 2 non-int (a MagicMock attr or a float) */
        if (cap_kind == 1 && cap_value > base) return json_number(cap_value);
    }
    return json_number(base);
}
