/*
 * telegram.c -- real C11 port of the query shaping and profile normalization of
 * upstream Hermes persistence/sqlite/conversation/_store/telegram.py.
 *
 * PoP: w21_telegram_15532d_normalize_profile @ persistence/sqlite/conversation/_store/telegram.py:_normalize_telegram_topic_profile_name
 * PoP: w21_telegram_15532d_params @ persistence/sqlite/conversation/_store/telegram.py:list_unlinked_telegram_sessions_for_user
 * PoP: w21_telegram_15532d_unlinked_query @ persistence/sqlite/conversation/_store/telegram.py:_UNLINKED_SELECT_HEAD
 *
 * `_normalize_telegram_topic_profile_name` is pure.  Everything else in the
 * module runs against a live `StoreHolder` (`holder._read_ctx`, the cursor, the
 * `telegram_dm_topic_bindings` table and the `sqlite3.OperationalError`
 * fallback), so what is ported here is the SQL text and the bind tuple the
 * loader hands to `conn.execute`, byte for byte, including the indentation the
 * module's f-string blocks carry.
 */
#include "hermes_json.h"
#include "noc_w21_telegram_15532d.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python's str.strip() default: space, \t, \n, \r, \v, \f. */
static int tg155_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *tg155_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python `str(x)` for the scalars this module's arguments carry. */
static char *tg155_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return tg155_dup("None");
    if (v->type == JSON_BOOL) return tg155_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return tg155_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return tg155_dup(buf);
    }
    /* str(list)/str(dict) can never strip to empty, but the module's parameter
     * is `Optional[str]`; containers are out of contract and read as empty. */
    return tg155_dup("");
}

/* Truthiness of the decoded argument: what `profile_name or ""` tests. */
static int tg155_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

char *w21_telegram_15532d_normalize_profile(const json_t *profile_name) {
    char *s;
    size_t start = 0, end;
    if (!tg155_truthy(profile_name)) return tg155_dup("default");
    s = tg155_str(profile_name);
    if (!s) return NULL;
    end = strlen(s);
    while (start < end && tg155_space((unsigned char)s[start])) start++;
    while (end > start && tg155_space((unsigned char)s[end - 1])) end--;
    memmove(s, s + start, end - start);
    s[end - start] = '\0';
    if (!s[0]) {
        free(s);
        return tg155_dup("default");
    }
    return s;
}

/* Python int() for the `limit` argument: bool, finite number truncated toward
 * zero, or a whitespace-trimmed numeric string.  On failure `*err` names the
 * exception class Python raises. */
static int tg155_to_int(const json_t *v, long long *out, const char **err) {
    if (!v || v->type == JSON_NULL) { *err = "TypeError"; return 0; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) { *err = "ValueError"; return 0; }
        if (d < -9223372036854775808.0 || d >= 9223372036854775808.0) { *err = "ValueError"; return 0; }
        *out = (long long)d;
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        const char *p = s;
        long long acc = 0;
        int neg = 0, digits = 0;
        while (*p && tg155_space((unsigned char)*p)) p++;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        while (*p >= '0' && *p <= '9') {
            if (acc > (9223372036854775807LL - (*p - '0')) / 10) { *err = "ValueError"; return 0; }
            acc = acc * 10 + (*p - '0');
            p++;
            digits++;
        }
        if (!digits) { *err = "ValueError"; return 0; }
        while (*p && tg155_space((unsigned char)*p)) p++;
        if (*p) { *err = "ValueError"; return 0; }
        *out = neg ? -acc : acc;
        return 1;
    }
    *err = "TypeError";
    return 0;
}

json_t *w21_telegram_15532d_params(const json_t *user_id, const json_t *profile_name,
                                   const json_t *limit) {
    json_t *res = json_object();
    json_t *arr;
    char *uid, *prof;
    long long lim = 0;
    const char *err = NULL;
    if (!res) return NULL;
    if (!tg155_to_int(limit, &lim, &err)) {
        json_set(res, "status", json_string("error"));
        json_set(res, "type", json_string(err ? err : "ValueError"));
        return res;
    }
    uid = tg155_str(user_id);
    prof = w21_telegram_15532d_normalize_profile(profile_name);
    arr = json_array();
    json_append(arr, json_string(uid ? uid : ""));
    json_append(arr, json_string(prof ? prof : "default"));
    json_append(arr, json_number((double)lim));
    free(uid);
    free(prof);
    json_set(res, "status", json_string("ok"));
    json_set(res, "value", arr);
    return res;
}

/* _UNLINKED_SELECT_HEAD, split around its two interpolated helper
 * expressions (`_sql_preview_raw()` and `_sql_session_last_active("s")`). */
static const char TG155_HEAD_0[] =
"\n                    SELECT s.*,\n                        COALESCE(sp.prompt, s.system_prompt)\n                            AS _system_prompt_resolved,\n                        ";
static const char TG155_HEAD_1[] =
",\n                        ";
static const char TG155_HEAD_2[] =
" AS last_active\n                    FROM sessions s\n                    LEFT JOIN system_prompts sp\n                      ON sp.hash = s.system_prompt_hash\n                    WHERE s.source = 'telegram'\n                      AND s.user_id = ?\n";

static const char TG155_SCOPE[] =
"                      AND COALESCE(NULLIF(TRIM(s.profile_name), ''), 'default') = ?\n                      AND NOT EXISTS (\n                          SELECT 1 FROM telegram_dm_topic_bindings b\n                          WHERE b.session_id = s.id\n                      )\n";

static const char TG155_TAIL[] =
"                    ORDER BY last_active DESC, s.started_at DESC\n                    LIMIT ?\n                    ";

json_t *w21_telegram_15532d_unlinked_query(int profile_scoped, const char *preview_expr,
                                           const char *last_active_expr) {
    size_t need, off = 0;
    char *buf;
    json_t *out;
    if (!preview_expr) preview_expr = "";
    if (!last_active_expr) last_active_expr = "";
    need = strlen(TG155_HEAD_0) + strlen(preview_expr) + strlen(TG155_HEAD_1)
         + strlen(last_active_expr) + strlen(TG155_HEAD_2) + strlen(TG155_TAIL) + 1;
    if (profile_scoped) need += strlen(TG155_SCOPE);
    buf = (char *)malloc(need);
    if (!buf) return NULL;
    off += (size_t)snprintf(buf + off, need - off, "%s", TG155_HEAD_0);
    off += (size_t)snprintf(buf + off, need - off, "%s", preview_expr);
    off += (size_t)snprintf(buf + off, need - off, "%s", TG155_HEAD_1);
    off += (size_t)snprintf(buf + off, need - off, "%s", last_active_expr);
    off += (size_t)snprintf(buf + off, need - off, "%s", TG155_HEAD_2);
    if (profile_scoped)
        off += (size_t)snprintf(buf + off, need - off, "%s", TG155_SCOPE);
    snprintf(buf + off, need - off, "%s", TG155_TAIL);
    out = json_string(buf);
    free(buf);
    return out ? out : json_null();
}
