/*
 * telegram_ids.c - C11 port of upstream plugins/platforms/telegram/telegram_ids.py
 * (gate w24_telegramid_756e5b).
 *
 * PoP: w24_telegramid_756e5b_normalize_chat_id @ plugins/platforms/telegram/telegram_ids.py:normalize_telegram_chat_id
 * PoP: w24_telegramid_756e5b_looks_like_username @ plugins/platforms/telegram/telegram_ids.py:looks_like_telegram_username
 * PoP: w24_telegramid_756e5b_parse_username_target @ plugins/platforms/telegram/telegram_ids.py:parse_telegram_username_target
 * PoP: w24_telegramid_756e5b_parse_send_target @ plugins/platforms/telegram/telegram_ids.py:parse_send_target
 * PoP: w24_telegramid_756e5b_parse_message_id @ plugins/platforms/telegram/telegram_ids.py:parse_telegram_message_id
 * PoP: w24_telegramid_756e5b_constants @ plugins/platforms/telegram/telegram_ids.py:<literal patterns>
 *
 * There is no I/O in this module upstream, so the port is one-for-one: the
 * Bot API id grammar (optional sign, digits, single inner underscores), the
 * ``@[A-Za-z0-9_]{4,32}`` username shape, the two send-target fullmatches and
 * the ``isdigit``/``> 0`` anchor rule.  The only seam is the argument type of
 * ``parse_send_target``: the send_message contract types the ref as ``str``,
 * so a non-string ref is not modelled.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_telegramid_756e5b.h"

/* ---- Python str() over the shapes a chat_id / anchor can hold ---------- */

static char *tid_strdup(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

static char *tid_pystr(const json_t *v) {
    char buf[64];
    if (!v) return tid_strdup("None");
    switch (v->type) {
    case JSON_NULL:   return tid_strdup("None");
    case JSON_BOOL:   return tid_strdup(v->bool_val ? "True" : "False");
    case JSON_STRING: return tid_strdup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return tid_strdup(buf);
    default: return tid_strdup("");   /* containers: not a chat-id shape */
    }
}

/* str.strip() with the ASCII whitespace set. */
static void tid_strip(const char *s, const char **begin, size_t *len) {
    const char *b = s ? s : "";
    const char *e = b + strlen(b);
    while (b < e && isspace((unsigned char)*b)) b++;
    while (e > b && isspace((unsigned char)e[-1])) e--;
    *begin = b;
    *len = (size_t)(e - b);
}

static int tid_is_digit(char c) { return c >= '0' && c <= '9'; }

/* Python int(text): [sign] digits with single inner underscores, whole string. */
static int tid_int_parse(const char *b, size_t n, long long *out) {
    size_t i = 0;
    int neg = 0, digits = 0, prev_digit = 0;
    long long acc = 0;
    if (n == 0) return 0;
    if (b[i] == '+' || b[i] == '-') { neg = (b[i] == '-'); i++; if (i == n) return 0; }
    for (; i < n; i++) {
        if (tid_is_digit(b[i])) {
            if (acc > (long long)1e17) return 0;   /* beyond exact-double range: not a fixture shape */
            acc = acc * 10 + (b[i] - '0');
            digits++;
            prev_digit = 1;
        } else if (b[i] == '_' && prev_digit && i + 1 < n && tid_is_digit(b[i + 1])) {
            prev_digit = 0;
        } else {
            return 0;
        }
    }
    if (!digits || !prev_digit) return 0;
    *out = neg ? -acc : acc;
    return 1;
}

/* ---- the two send-target fullmatches -------------------------------- */

/* ^\s*(-?\d+)(?::(\d+))?\s*$ */
static int tid_match_numeric_target(const char *b, size_t n, size_t *c0, size_t *c1,
                                   int *has_topic, size_t *t0, size_t *t1) {
    size_t i = 0, s;
    while (i < n && isspace((unsigned char)b[i])) i++;
    s = i;
    if (i < n && b[i] == '-') i++;
    if (i >= n || !tid_is_digit(b[i])) return 0;
    while (i < n && tid_is_digit(b[i])) i++;
    *c0 = s; *c1 = i;
    *has_topic = 0;
    if (i < n && b[i] == ':') {
        size_t ts = ++i;
        if (i >= n || !tid_is_digit(b[i])) return 0;
        while (i < n && tid_is_digit(b[i])) i++;
        *t0 = ts; *t1 = i;
        *has_topic = 1;
    }
    while (i < n && isspace((unsigned char)b[i])) i++;
    return i == n;
}

/* ^\s*(\d+):([^:\s][^:]*?)\s*$ */
static int tid_match_named_topic(const char *b, size_t n, size_t *c0, size_t *c1,
                                 size_t *t0, size_t *t1) {
    size_t i = 0, s, ts, end;
    while (i < n && isspace((unsigned char)b[i])) i++;
    s = i;
    if (i >= n || !tid_is_digit(b[i])) return 0;
    while (i < n && tid_is_digit(b[i])) i++;
    *c0 = s; *c1 = i;
    if (i >= n || b[i] != ':') return 0;
    ts = ++i;
    if (i >= n || b[i] == ':' || isspace((unsigned char)b[i])) return 0;
    end = n;
    while (end > ts && isspace((unsigned char)b[end - 1])) end--;
    for (i = ts; i < end; i++)
        if (b[i] == ':') return 0;
    *t0 = ts; *t1 = end;
    return 1;
}

static json_t *tid_pair(const char *b, size_t c0, size_t c1, int has_topic,
                        size_t t0, size_t t1) {
    char *a = (char *)malloc(c1 - c0 + 1);
    json_t *arr = json_array();
    if (!a || !arr) { free(a); json_free(arr); return json_null(); }
    memcpy(a, b + c0, c1 - c0);
    a[c1 - c0] = '\0';
    json_append(arr, json_string(a));
    free(a);
    if (has_topic) {
        char *t = (char *)malloc(t1 - t0 + 1);
        if (t) {
            memcpy(t, b + t0, t1 - t0);
            t[t1 - t0] = '\0';
            json_append(arr, json_string(t));
            free(t);
        } else {
            json_append(arr, json_null());
        }
    } else {
        json_append(arr, json_null());
    }
    return arr;
}

/* ---- exported port -------------------------------------------------- */

json_t *w24_telegramid_756e5b_normalize_chat_id(const json_t *value) {
    char *s = tid_pystr(value);
    const char *b;
    size_t n;
    long long num;
    json_t *out;
    if (!s) return json_null();
    tid_strip(s, &b, &n);
    if (tid_int_parse(b, n, &num)) {
        out = json_number((double)num);
    } else {
        char *copy = (char *)malloc(n + 1);
        if (!copy) { free(s); return json_null(); }
        memcpy(copy, b, n);
        copy[n] = '\0';
        out = json_string(copy);
        free(copy);
    }
    free(s);
    return out;
}

/* @[A-Za-z0-9_]{4,32}, fullmatch */
static int tid_username_shape(const char *s, size_t n) {
    size_t i;
    if (n < 1 + W24_TELEGRAMID_756E5B_USERNAME_MIN) return 0;
    if (n > 1 + W24_TELEGRAMID_756E5B_USERNAME_MAX) return 0;
    if (s[0] != '@') return 0;
    for (i = 1; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

json_t *w24_telegramid_756e5b_looks_like_username(const json_t *value) {
    char *s = tid_pystr(value);
    const char *b;
    size_t n;
    int ok;
    if (!s) return json_bool(0);
    tid_strip(s, &b, &n);
    ok = tid_username_shape(b, n);
    free(s);
    return json_bool(ok);
}

json_t *w24_telegramid_756e5b_parse_username_target(const json_t *value) {
    char *s = tid_pystr(value);
    const char *b;
    size_t n;
    json_t *out;
    if (!s) return json_null();
    tid_strip(s, &b, &n);
    if (tid_username_shape(b, n)) {
        char *copy = (char *)malloc(n + 1);
        if (!copy) { free(s); return json_null(); }
        memcpy(copy, b, n);
        copy[n] = '\0';
        out = json_string(copy);
        free(copy);
    } else {
        out = json_null();
    }
    free(s);
    return out;
}

json_t *w24_telegramid_756e5b_parse_send_target(const json_t *ref) {
    const char *raw;
    size_t n, c0 = 0, c1 = 0, t0 = 0, t1 = 0;
    int has_topic = 0;
    json_t *user;
    if (!ref || ref->type != JSON_STRING || !ref->str_val) return json_null();
    raw = ref->str_val;
    n = strlen(raw);
    if (tid_match_numeric_target(raw, n, &c0, &c1, &has_topic, &t0, &t1))
        return tid_pair(raw, c0, c1, has_topic, t0, t1);
    if (tid_match_named_topic(raw, n, &c0, &c1, &t0, &t1))
        return tid_pair(raw, c0, c1, 1, t0, t1);
    user = w24_telegramid_756e5b_parse_username_target(ref);
    if (user && user->type == JSON_STRING) {
        json_t *arr = json_array();
        json_append(arr, user);
        json_append(arr, json_null());
        return arr;
    }
    json_free(user);
    return json_null();
}

json_t *w24_telegramid_756e5b_parse_message_id(const json_t *value) {
    if (!value || value->type == JSON_NULL) return json_null();
    if (value->type == JSON_BOOL) return json_null();          /* isinstance(value, bool) -> None */
    if (value->type == JSON_NUMBER) {
        if (value->num_val == floor(value->num_val) && value->num_val > 0)
            return json_number(value->num_val);
        return json_null();
    }
    if (value->type == JSON_STRING && value->str_val) {
        const char *b;
        size_t n, i;
        long long num;
        tid_strip(value->str_val, &b, &n);
        if (n == 0) return json_null();
        for (i = 0; i < n; i++)
            if (!tid_is_digit(b[i])) return json_null();        /* isascii() and isdigit() */
        if (!tid_int_parse(b, n, &num) || num <= 0) return json_null();
        return json_number((double)num);
    }
    return json_null();
}

json_t *w24_telegramid_756e5b_constants(void) {
    json_t *o = json_object();
    json_set(o, "username_min", json_number((double)W24_TELEGRAMID_756E5B_USERNAME_MIN));
    json_set(o, "username_max", json_number((double)W24_TELEGRAMID_756E5B_USERNAME_MAX));
    json_set(o, "username_pattern", json_string("@[A-Za-z0-9_]{4,32}"));
    json_set(o, "send_target_pattern", json_string("^\\s*(-?\\d+)(?::(\\d+))?\\s*$"));
    json_set(o, "named_dm_topic_pattern", json_string("^\\s*(\\d+):([^:\\s][^:]*?)\\s*$"));
    return o;
}
