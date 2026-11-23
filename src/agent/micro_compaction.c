/*
 * micro_compaction.c — C port of upstream Hermes agent/micro_compaction.py.
 *
 * PoP: w8_microcompac_666e99_is_summary_marker @ agent/micro_compaction.py:_is_summary_marker
 * PoP: w8_microcompac_666e99_is_micro_marker @ agent/micro_compaction.py:_is_micro_marker
 * PoP: w8_microcompac_666e99_rolling_summary_from_marker @ agent/micro_compaction.py:MicroCompactionMixin._rolling_summary_from_marker
 * PoP: w8_microcompac_666e99_render_micro_marker_content @ agent/micro_compaction.py:MicroCompactionMixin._render_micro_marker_content
 * PoP: w8_microcompac_666e99_record_micro_failure @ agent/micro_compaction.py:MicroCompactionMixin._record_micro_failure
 * PoP: w8_microcompac_666e99_reset_micro_failure_tracking @ agent/micro_compaction.py:MicroCompactionMixin._reset_micro_failure_tracking
 * PoP: w8_microcompac_666e99_find_one_exchange @ agent/micro_compaction.py:MicroCompactionMixin._find_one_exchange
 *
 * context_compressor constants and _is_context_summary_message are injected
 * from the fixture (attributes are read at call time in the Python source).
 */
#include "noc_w8_microcompac_666e99.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: return v->c.count > 0 ? 1 : 0;
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}
static const char *sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static char *strip_dup(const char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}
static long find_sub(const char *hay, const char *needle) {
    if (!needle || !*needle) return 0;
    const char *p = strstr(hay, needle);
    return p ? (long)(p - hay) : -1;
}
static long rfind_sub(const char *hay, const char *needle) {
    if (!needle || !*needle) return (long)strlen(hay);
    size_t n = strlen(hay), m = strlen(needle);
    if (m > n) return -1;
    for (size_t i = n - m + 1; i > 0; i--) {
        if (strncmp(hay + i - 1, needle, m) == 0) return (long)(i - 1);
    }
    return -1;
}

int w8_microcompac_666e99_is_summary_marker(const struct json_t *args) {
    const json_t *entry = json_obj_get(args, "entry");
    const json_t *cc = json_obj_get(args, "cc");
    if (!entry || entry->type != JSON_OBJECT) return 0;
    return truthy(json_obj_get(entry, json_get_str(cc, "summary_key", ""))) ? 1 : 0;
}
int w8_microcompac_666e99_is_micro_marker(const struct json_t *args) {
    if (!w8_microcompac_666e99_is_summary_marker(args)) return 0;
    const json_t *entry = json_obj_get(args, "entry");
    const json_t *cc = json_obj_get(args, "cc");
    return truthy(json_obj_get(entry, json_get_str(cc, "micro_key", ""))) ? 1 : 0;
}

json_t *w8_microcompac_666e99_rolling_summary_from_marker(const struct json_t *args) {
    const json_t *v = json_obj_get(args, "content");
    const json_t *cc = json_obj_get(args, "cc");
    if (!v || v->type != JSON_STRING || !v->str_val) return json_string("");
    const char *content = v->str_val;
    char *probe = strip_dup(content);
    if (!probe || !*probe) { free(probe); return json_string(""); }
    free(probe);

    const char *heading = json_get_str(cc, "heading", "");
    const char *end_marker = json_get_str(cc, "end_marker", "");
    long idx = rfind_sub(content, heading);
    const char *body = content;
    if (idx >= 0) body = content + idx + strlen(heading);

    long e = find_sub(body, end_marker);
    size_t n = (e >= 0) ? (size_t)e : strlen(body);
    char *tmp = (char *)malloc(n + 1);
    if (!tmp) return json_string("");
    memcpy(tmp, body, n);
    tmp[n] = '\0';
    char *out = strip_dup(tmp);
    free(tmp);
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

json_t *w8_microcompac_666e99_render_micro_marker_content(const struct json_t *args) {
    const json_t *cc = json_obj_get(args, "cc");
    const char *prefix = json_get_str(cc, "summary_prefix", "");
    const char *heading = json_get_str(cc, "heading", "");
    const char *end = json_get_str(cc, "end_marker", "");
    char *st = strip_dup(sval(json_obj_get(args, "summary_text")));
    size_t n = strlen(prefix) + 2 + strlen(heading) + 1 + (st ? strlen(st) : 0) + 2 + strlen(end) + 1;
    char *buf = (char *)malloc(n);
    if (!buf) { free(st); return json_string(""); }
    snprintf(buf, n, "%s\n\n%s\n%s\n\n%s", prefix, heading, st ? st : "", end);
    free(st);
    json_t *r = json_string(buf);
    free(buf);
    return r;
}

static long get_int(const json_t *obj, const char *key, long def) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!v || v->type != JSON_NUMBER) return def;
    return (long)v->num_val;
}

json_t *w8_microcompac_666e99_record_micro_failure(const struct json_t *args) {
    long start = get_int(args, "exchange_start", 0);
    long end = get_int(args, "exchange_end", 0);
    const json_t *state = json_obj_get(args, "state");
    const json_t *cc = json_obj_get(args, "cc");
    long max_failures = get_int(cc, "max_failures", 3);
    long cons = get_int(state, "_micro_compact_consecutive_failures", 0);
    long last = get_int(state, "_micro_compact_last_failure_cursor", -1);
    long cursor = get_int(state, "_micro_compact_cursor", 0);

    int same = (start == last);
    cons = same ? cons + 1 : 1;
    last = start;
    const char *outcome;
    if (cons < max_failures) {
        outcome = "summarize_failed";
    } else {
        cursor = end;
        cons = 0;
        last = -1;
        outcome = "exchange_skipped";
    }
    json_t *r = json_object();
    json_set(r, "outcome", json_string(outcome));
    json_t *st = json_object();
    json_set(st, "_micro_compact_consecutive_failures", json_number((double)cons));
    json_set(st, "_micro_compact_last_failure_cursor", json_number((double)last));
    json_set(st, "_micro_compact_cursor", json_number((double)cursor));
    json_set(r, "state", st);
    return r;
}

json_t *w8_microcompac_666e99_reset_micro_failure_tracking(const struct json_t *state) {
    json_t *r = json_object();
    json_set(r, "_micro_compact_consecutive_failures", json_number(0));
    json_set(r, "_micro_compact_last_failure_cursor", json_number(-1));
    json_set(r, "_micro_compact_cursor",
             json_number((double)get_int(state, "_micro_compact_cursor", 0)));
    return r;
}

static int is_summary_msg(const json_t *m) {
    return m && m->type == JSON_OBJECT && truthy(json_obj_get(m, "_summary"));
}
static int turn_row(const json_t *messages, size_t idx, int allow_tool) {
    if (!messages || idx >= messages->c.count) return 0;
    const json_t *m = messages->c.items[idx];
    if (!m || m->type != JSON_OBJECT || is_summary_msg(m)) return 0;
    const char *role = json_get_str(m, "role", "");
    return (!strcmp(role, "assistant") || (allow_tool && !strcmp(role, "tool"))) ? 1 : 0;
}

json_t *w8_microcompac_666e99_find_one_exchange(const struct json_t *args) {
    const json_t *messages = json_obj_get(args, "messages");
    long start_l = get_int(args, "start", 0);
    long tail_l = get_int(args, "tail_start", 0);
    if (!messages || messages->type != JSON_ARRAY) return json_null();
    size_t n = messages->c.count;
    size_t limit = (tail_l < (long)n) ? (size_t)(tail_l > 0 ? tail_l : 0) : n;
    size_t idx = (start_l > 0) ? (size_t)start_l : 0;
    if (idx > n) idx = n;
    while (idx < limit && !turn_row(messages, idx, 0)) idx++;
    if (idx >= limit) return json_null();
    size_t exchange_start = idx;
    idx++;
    while (idx < limit && turn_row(messages, idx, 1)) idx++;

    const json_t *boundary = (idx < n) ? messages->c.items[idx] : NULL;
    if (!boundary || boundary->type != JSON_OBJECT) return json_null();
    const char *role = json_get_str(boundary, "role", "");
    if (!strcmp(role, "assistant") || !strcmp(role, "tool")) return json_null();

    json_t *r = json_array();
    json_append(r, json_number((double)exchange_start));
    json_append(r, json_number((double)idx));
    return r;
}
