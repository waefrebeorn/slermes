/*
 * adapter_config_readers.c — C11 port of upstream Hermes
 * plugins/platforms/discord/adapter_config_readers.py (gate w25_adaptercon_1cab56).
 * STATUS: real behaviour port; reading config.yaml is a seam (the raw value is
 *         the input), so nothing here opens a file.
 * PoP: w25_adaptercon_1cab56_dm_role_auth_guild @ plugins/platforms/discord/adapter_config_readers.py:_read_dm_role_auth_guild
 * PoP: w25_adaptercon_1cab56_discord_prompt_timeout @ plugins/platforms/discord/adapter_config_readers.py:_read_discord_prompt_timeout
 * PoP: w25_adaptercon_1cab56_exec_approval_gate @ plugins/platforms/discord/adapter_config_readers.py:_resolve_exec_approval_admin_gate
 * PoP: w25_adaptercon_1cab56_coerce_id_list @ gateway/slash_access.py:_coerce_id_list
 * PoP: w25_adaptercon_1cab56_constants @ plugins/platforms/discord/adapter_config_readers.py:_DISCORD_PROMPT_TIMEOUT_*
 *
 * Everything here is what the values decide: None/"" fail to a secure default,
 * a non-numeric string falls back, the button-view timeout is kept bounded, and
 * the admin gate fails closed when it is on with no admins.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_adaptercon_1cab56.h"

/* _DISCORD_PROMPT_TIMEOUT_DEFAULT / _MIN / _MAX (int(MAX_SAFE_TIMEOUT_S)) */
#define AC_PROMPT_TIMEOUT_DEFAULT 300.0
#define AC_PROMPT_TIMEOUT_MIN 30.0
#ifdef _WIN32
/* agent/deadline.py: (0xFFFFFFFF - 1000) / 1000.0 on win32 */
#define AC_PROMPT_TIMEOUT_MAX 4294966.0
#else
/* agent/deadline.py: 31_536_000.0 (365 days) off-Windows */
#define AC_PROMPT_TIMEOUT_MAX 31536000.0
#endif

static int ac_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* int(s) for a str: surrounding whitespace, an optional sign, digits with
 * single underscores between them.  Anything else is ValueError. */
static int ac_int_from_str(const char *s, long long *out) {
    size_t i = 0, n = strlen(s), start;
    int neg = 0;
    long long acc = 0;
    int prev_digit = 0;

    while (i < n && ac_ws(s[i])) i++;
    while (n > i && ac_ws(s[n - 1])) n--;
    if (i >= n) return 0;
    if (s[i] == '+' || s[i] == '-') {
        neg = s[i] == '-';
        i++;
    }
    start = i;
    for (; i < n; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            if (acc > (long long)922337203685477580LL) return 0; /* overflow guard */
            acc = acc * 10 + (c - '0');
            prev_digit = 1;
        } else if (c == '_' && prev_digit && i + 1 < n &&
                   s[i + 1] >= '0' && s[i + 1] <= '9') {
            prev_digit = 0;
        } else {
            return 0;
        }
    }
    if (i == start) return 0; /* a bare sign is not an int */
    *out = neg ? -acc : acc;
    return 1;
}

/* int(value): bools are ints, floats truncate toward zero, strings parse. */
static int ac_py_int(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;         /* TypeError */
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d != d || d > 9.2e18 || d < -9.2e18) return 0; /* inf/nan: OverflowError */
        *out = (long long)d;                              /* truncation -> 0 */
        return 1;
    }
    if (v->type == JSON_STRING) return ac_int_from_str(v->str_val ? v->str_val : "", out);
    return 0;                                             /* TypeError */
}

json_t *w25_adaptercon_1cab56_dm_role_auth_guild(const json_t *raw) {
    long long guild;
    if (!raw || raw->type == JSON_NULL) return json_null();
    if (raw->type == JSON_STRING && (!raw->str_val || raw->str_val[0] == '\0'))
        return json_null(); /* raw == "" */
    if (!ac_py_int(raw, &guild)) return json_null();
    return guild > 0 ? json_number((double)guild) : json_null();
}

json_t *w25_adaptercon_1cab56_discord_prompt_timeout(const json_t *raw) {
    long long seconds;
    if (!raw || raw->type == JSON_NULL) return json_number(AC_PROMPT_TIMEOUT_DEFAULT);
    if (raw->type == JSON_STRING && (!raw->str_val || raw->str_val[0] == '\0'))
        return json_number(AC_PROMPT_TIMEOUT_DEFAULT);
    if (!ac_py_int(raw, &seconds)) return json_number(AC_PROMPT_TIMEOUT_DEFAULT);
    if ((double)seconds < AC_PROMPT_TIMEOUT_MIN) return json_number(AC_PROMPT_TIMEOUT_MIN);
    if ((double)seconds > AC_PROMPT_TIMEOUT_MAX) return json_number(AC_PROMPT_TIMEOUT_MAX);
    return json_number((double)seconds);
}

/* str(value) as the gate's toggle comparison sees it. */
static json_t *ac_str_of(const json_t *v) {
    char buf[64];
    if (!v) return json_string("None");
    switch (v->type) {
    case JSON_NULL:   return json_string("None");
    case JSON_STRING: return json_string(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return json_string(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return json_string(buf);
    case JSON_ARRAY:  return json_string("[]");
    default:          return json_string("{}");
    }
}

/* str(x).strip().lower() */
static char *ac_norm(const char *s) {
    size_t n = strlen(s), b = 0, e = n, i;
    char *buf;
    while (b < e && ac_ws(s[b])) b++;
    while (e > b && ac_ws(s[e - 1])) e--;
    buf = (char *)malloc(e - b + 1);
    if (!buf) return NULL;
    for (i = b; i < e; i++) {
        char c = s[i];
        buf[i - b] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    buf[e - b] = '\0';
    return buf;
}

static int ac_toggle_on(const json_t *raw) {
    json_t *s = ac_str_of(raw);
    char *norm;
    int on = 0;
    if (!s || s->type != JSON_STRING) { json_free(s); return 0; }
    norm = ac_norm(s->str_val ? s->str_val : "");
    if (norm) {
        on = !strcmp(norm, "true") || !strcmp(norm, "1") || !strcmp(norm, "yes");
        free(norm);
    }
    json_free(s);
    return on;
}

/* str(it).strip(), empty dropped: the per-item normalization.  Takes the
 * node by ownership and frees it. */
static int ac_push_norm(json_t *arr, json_t *raw) {
    json_t *s = ac_str_of(raw);
    const char *buf;
    size_t n, b, e;
    char *norm;
    if (!s || s->type != JSON_STRING) { json_free(s); json_free(raw); return 0; }
    buf = s->str_val ? s->str_val : "";
    n = strlen(buf);
    b = 0;
    e = n;
    while (b < e && ac_ws(buf[b])) b++;
    while (e > b && ac_ws(buf[e - 1])) e--;
    if (e == b) { json_free(s); json_free(raw); return 0; } /* "" is dropped */
    norm = (char *)malloc(e - b + 1);
    if (!norm) { json_free(s); json_free(raw); return 0; }
    if (e > b) memcpy(norm, buf + b, e - b);
    norm[e - b] = '\0';
    json_append(arr, json_string(norm));
    free(norm);
    json_free(s);
    json_free(raw);
    return 1;
}

static int ac_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* _coerce_list(raw) -> frozenset[str], exposed as a sorted unique list. */
static void ac_coerce(const json_t *raw, json_t *array) {
    if (!raw || raw->type == JSON_NULL) return;
    if (raw->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < json_len(raw); i++) {
            const json_t *item = json_get(raw, i);
            ac_push_norm(array, item ? json_copy(item) : json_null());
        }
        return;
    }
    if (raw->type == JSON_STRING) {
        const char *s = raw->str_val ? raw->str_val : "";
        const char *start = s, *p = s;
        for (;;) {
            if (*p == ',' || *p == '\0') {
                size_t len = (size_t)(p - start);
                if (len > 0) {
                    char *piece = (char *)malloc(len + 1);
                    if (piece) {
                        memcpy(piece, start, len);
                        piece[len] = '\0';
                        ac_push_norm(array, json_string(piece));
                        free(piece);
                    }
                }
                if (*p == '\0') break;
                start = p + 1;
            }
            p++;
        }
        return;
    }
    ac_push_norm(array, json_copy(raw)); /* a single scalar (int user id, etc.) */
}

/* Sort + dedupe in place: the frozenset's member set, in one stable order. */
static json_t *ac_sorted_unique(json_t *array) {
    size_t n = json_len(array), i, out_n = 0;
    const char **vals;
    json_t *out;

    if (n == 0) { json_free(array); return json_array(); }
    vals = (const char **)calloc(n, sizeof(const char *));
    if (!vals) { json_free(array); return json_array(); }
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(array, i);
        vals[i] = v && v->type == JSON_STRING && v->str_val ? v->str_val : "";
    }
    qsort(vals, n, sizeof(const char *), ac_cmp_str);
    out = json_array();
    for (i = 0; i < n; i++) {
        if (i > 0 && strcmp(vals[i], vals[i - 1]) == 0) continue;
        json_append(out, json_string(vals[i]));
        out_n++;
    }
    (void)out_n;
    free(vals);
    json_free(array);
    return out;
}

json_t *w25_adaptercon_1cab56_coerce_id_list(const json_t *raw) {
    json_t *array = json_array();
    ac_coerce(raw, array);
    return ac_sorted_unique(array);
}

json_t *w25_adaptercon_1cab56_exec_approval_gate(const json_t *extra) {
    const json_t *cfg = extra && extra->type == JSON_OBJECT ? extra : NULL;
    json_t *out = json_object();
    const json_t *toggle = cfg ? json_obj_get(cfg, "require_admin_for_exec_approval") : NULL;
    const json_t *admins = cfg ? json_obj_get(cfg, "allow_admin_from") : NULL;

    if (!out) return json_null();
    if (!ac_toggle_on(toggle)) {
        json_set(out, "require_admin", json_bool(0));
        json_set(out, "admin_user_ids", json_array());
        return out;
    }
    json_set(out, "require_admin", json_bool(1));
    json_set(out, "admin_user_ids", w25_adaptercon_1cab56_coerce_id_list(admins));
    return out;
}

json_t *w25_adaptercon_1cab56_constants(void) {
    json_t *o = json_object();
    json_set(o, "prompt_timeout_default", json_number(AC_PROMPT_TIMEOUT_DEFAULT));
    json_set(o, "prompt_timeout_min", json_number(AC_PROMPT_TIMEOUT_MIN));
    json_set(o, "prompt_timeout_max", json_number(AC_PROMPT_TIMEOUT_MAX));
    json_set(o, "dm_role_auth_guild_key", json_string("dm_role_auth_guild"));
    json_set(o, "prompt_timeout_key", json_string("discord_prompt_timeout"));
    json_set(o, "require_admin_key", json_string("require_admin_for_exec_approval"));
    json_set(o, "allow_admin_from_key", json_string("allow_admin_from"));
    return o;
}
