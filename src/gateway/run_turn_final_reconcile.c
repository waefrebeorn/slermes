/*
 * run_turn_final_reconcile.c - C11 port of upstream
 * gateway/run_turn_final_reconcile.py (gate w26_runturnfin_c00b96).
 *
 * PoP: w26_runturnfin_c00b96_flood_wait @ gateway/run_turn_final_reconcile.py:_flood_wait
 * PoP: w26_runturnfin_c00b96_carries_whole @ gateway/run_turn_final_reconcile.py:_edit_carries_whole
 * PoP: w26_runturnfin_c00b96_owns_preview @ gateway/run_turn_final_reconcile.py:owns_reconcilable_preview
 * PoP: w26_runturnfin_c00b96_constants @ gateway/run_turn_final_reconcile.py:<retry budget>
 *
 * `edit_preview_to_final` waits and awaits; the adapter, its length units and
 * the platform's flood wording are seams. What is ported is what those values
 * decide: how long a flood refusal asks to wait, whether one finalize edit can
 * show all of the answer, and whether the consumer still owns one real preview.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_runturnfin_c00b96.h"

#define RTC_FLOOD_PREFIX "flood_control:"
#define RTC_FLOOD_DEFAULT 60.0
#define RTC_MAX_FLOOD_WAITS 2
#define RTC_FLOOD_WAIT_SLACK 1.0
#define RTC_SEND_RETRY_INLINE_CAP 60.0
#define RTC_DEFAULT_MAX_MESSAGE_LENGTH 4096

/* Python truthiness of a JSON value. */
static int rtc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static char *rtc_lower_trim(const char *s) {
    size_t n, a = 0, b, i;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    while (a < n && isspace((unsigned char)s[a])) a++;
    b = n;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    for (i = a; i < b; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i - a] = (char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
    }
    out[b - a] = '\0';
    return out;
}

/* _RAW_FLOOD_RE: "flood control exceeded" ... "retry in <digits[.digits]>". */
static int rtc_raw_flood_wait(const char *text, double *out) {
    const char *hit, *retry, *p;
    char *end = NULL;
    double value;
    if (!text) return 0;
    hit = strstr(text, "flood control exceeded");
    if (!hit) return 0;
    retry = strstr(hit, "retry in");
    if (!retry) return 0;
    p = retry + strlen("retry in");
    while (*p && isspace((unsigned char)*p)) p++;
    if (!isdigit((unsigned char)*p)) return 0;
    value = strtod(p, &end);
    if (end == p) return 0;
    *out = value;
    return 1;
}

/* flood_wait_seconds(): the wait the platform asked for, default when unreadable. */
static double rtc_flood_wait_seconds(const char *text) {
    double raw = 0.0, wait = RTC_FLOOD_DEFAULT;
    if (text && !strncmp(text, RTC_FLOOD_PREFIX, strlen(RTC_FLOOD_PREFIX))) {
        const char *rest = text + strlen(RTC_FLOOD_PREFIX);
        char *end = NULL;
        while (*rest && isspace((unsigned char)*rest)) rest++;
        wait = strtod(rest, &end);
        /* float() consumes the whole string; anything else is unreadable. */
        if (end == rest || *end != '\0') wait = RTC_FLOOD_DEFAULT;
    } else if (rtc_raw_flood_wait(text, &raw)) {
        wait = raw;
    }
    return wait > 0.0 ? wait : RTC_FLOOD_DEFAULT;
}

json_t *w26_runturnfin_c00b96_flood_wait(const json_t *req) {
    const json_t *err = req ? json_obj_get(req, "error") : NULL;
    const json_t *retry_after = req ? json_obj_get(req, "retry_after") : NULL;
    char *text;
    int flood;

    if (err && err->type != JSON_STRING && err->type != JSON_NULL) return json_null();
    text = rtc_lower_trim(err && err->type == JSON_STRING ? err->str_val : "");
    if (!text) return json_null();
    flood = !strncmp(text, RTC_FLOOD_PREFIX, strlen(RTC_FLOOD_PREFIX));
    if (!flood) {
        double raw = 0.0;
        flood = rtc_raw_flood_wait(text, &raw);
    }
    if (!flood) { free(text); return json_null(); }
    if (retry_after && retry_after->type == JSON_NUMBER && retry_after->num_val > 0.0) {
        double v = retry_after->num_val;
        free(text);
        return json_number(v);
    }
    {
        double v = rtc_flood_wait_seconds(text);
        free(text);
        return json_number(v);
    }
}

/* The adapter's own length unit: code points, or UTF-16 code units. */
static size_t rtc_len(const char *s, const char *unit) {
    int utf16 = unit && !strcmp(unit, "utf16");
    size_t n = 0;
    if (!s) return 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x80) { s += 1; n += 1; }
        else if ((c & 0xE0) == 0xC0) { s += 2; n += 1; }
        else if ((c & 0xF0) == 0xE0) { s += 3; n += 1; }
        else if ((c & 0xF8) == 0xF0) { s += 4; n += utf16 ? 2 : 1; }
        else { s += 1; n += 1; }
    }
    return n;
}

/* Slack's formatter: & -> &amp; (the only shape this gate measures). */
static char *rtc_amp(const char *s) {
    size_t i, n = 0, j = 0;
    char *out;
    if (!s) s = "";
    for (i = 0; s[i]; i++) n += (s[i] == '&') ? 5 : 1;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; s[i]; i++) {
        if (s[i] == '&') { memcpy(out + j, "&amp;", 5); j += 5; }
        else out[j++] = s[i];
    }
    out[j] = '\0';
    return out;
}

/* _edit_carries_whole over the values the adapter would supply. */
static int rtc_carries_whole(const json_t *spec, const char *content) {
    const json_t *limit_v, *splits_v, *base_v, *fmt_v, *unit_v;
    const char *unit, *fmt;
    int base;
    double limit;

    if (!spec || spec->type != JSON_OBJECT) return 0;
    splits_v = json_obj_get(spec, "splits");
    if (splits_v && splits_v->type == JSON_BOOL && splits_v->bool_val) return 1;
    limit_v = json_obj_get(spec, "limit");
    base_v = json_obj_get(spec, "base");
    fmt_v = json_obj_get(spec, "fmt");
    unit_v = json_obj_get(spec, "len_fn");
    base = base_v && base_v->type == JSON_BOOL && base_v->bool_val;
    /* Only a BasePlatformAdapter supplies a per-chat length UNIT. A plain adapter
     * falls back to len(), i.e. code points -- so three emoji are length 3, not the
     * 6 UTF-16 units a per-chat unit would report. */
    unit = (base && unit_v && unit_v->type == JSON_STRING) ? unit_v->str_val : "len";
    fmt = (fmt_v && fmt_v->type == JSON_STRING) ? fmt_v->str_val : "none";
    /* getattr(adapter, "MAX_MESSAGE_LENGTH", 4096): absent means the default;
     * a present non-int limit never fits (isinstance(limit, int) is False). */
    if (!limit_v || limit_v->type == JSON_NULL) limit = (double)RTC_DEFAULT_MAX_MESSAGE_LENGTH;
    else if (limit_v->type == JSON_NUMBER) limit = limit_v->num_val;
    else return 0;
    if ((double)rtc_len(content, unit) > limit) return 0;
    if (!strcmp(fmt, "amp")) {
        char *formatted = rtc_amp(content);
        int fits;
        if (!formatted) return 0;
        fits = (double)rtc_len(formatted, unit) <= limit;
        free(formatted);
        return fits;
    }
    return 1;
}

json_t *w26_runturnfin_c00b96_carries_whole(const json_t *req) {
    const json_t *content = req ? json_obj_get(req, "content") : NULL;
    const char *text = (content && content->type == JSON_STRING) ? content->str_val : "";
    if (req) {
        int ok = rtc_carries_whole(req, text);
        return json_bool(ok ? true : false);
    }
    return json_bool(false);
}

json_t *w26_runturnfin_c00b96_owns_preview(const json_t *req) {
    const json_t *consumer = req ? json_obj_get(req, "consumer") : NULL;
    const json_t *content = req ? json_obj_get(req, "content") : NULL;
    const json_t *message_id, *adapter, *clean;
    static const char *const FLAGS[5] = {
        "native_streaming", "delivery_ambiguous", "delivery_uncertain",
        "preview_uncertain", "egress_declined",
    };
    size_t i;
    const char *text;

    if (!consumer || consumer->type != JSON_OBJECT) return json_bool(false);
    if (json_get_bool(consumer, "turn_split_delivery", false)) return json_bool(false);
    message_id = json_obj_get(consumer, "message_id");
    adapter = json_obj_get(consumer, "adapter");
    if (!rtc_truthy(message_id)) return json_bool(false);
    if (message_id->type == JSON_STRING && message_id->str_val
        && !strcmp(message_id->str_val, "__no_edit__")) return json_bool(false);
    if (!adapter || adapter->type != JSON_OBJECT) return json_bool(false);
    for (i = 0; i < 5; i++)
        if (json_get_bool(consumer, FLAGS[i], false)) return json_bool(false);
    if (!content || content->type == JSON_NULL) return json_bool(true);  /* content is None */

    text = content->type == JSON_STRING ? content->str_val : "";
    clean = json_obj_get(consumer, "clean");
    if (clean && clean->type == JSON_STRING && !strcmp(clean->str_val, "strip")) {
        /* _clean_for_display: the display text is what the edit must carry. */
        size_t a = 0, b = strlen(text);
        char *trimmed;
        int ok;
        while (text[a] == ' ' || text[a] == '\n' || text[a] == '\t' || text[a] == '\r') a++;
        while (b > a && (text[b - 1] == ' ' || text[b - 1] == '\n'
                         || text[b - 1] == '\t' || text[b - 1] == '\r')) b--;
        trimmed = (char *)malloc(b - a + 1);
        if (!trimmed) return json_bool(false);
        memcpy(trimmed, text + a, b - a);
        trimmed[b - a] = '\0';
        ok = rtc_carries_whole(adapter, trimmed);
        free(trimmed);
        return json_bool(ok ? true : false);
    }
    return json_bool(rtc_carries_whole(adapter, text) ? true : false);
}

json_t *w26_runturnfin_c00b96_constants(void) {
    json_t *o = json_object();
    json_set(o, "flood_error_prefix", json_string(RTC_FLOOD_PREFIX));
    json_set(o, "flood_retry_default_seconds", json_number(RTC_FLOOD_DEFAULT));
    json_set(o, "max_flood_waits", json_number((double)RTC_MAX_FLOOD_WAITS));
    json_set(o, "flood_wait_slack_secs", json_number(RTC_FLOOD_WAIT_SLACK));
    json_set(o, "send_retry_inline_wait_cap_secs", json_number(RTC_SEND_RETRY_INLINE_CAP));
    json_set(o, "default_max_message_length", json_number(RTC_DEFAULT_MAX_MESSAGE_LENGTH));
    json_set(o, "no_edit_marker", json_string("__no_edit__"));
    return o;
}
