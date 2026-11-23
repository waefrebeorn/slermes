/*
 * session.c — real C11 port of the pure helpers of upstream Hermes
 * acp_adapter/session.py (gate w6_session_5810af).
 *
 * PoP: acpsess_build_session_title @ acp_adapter/session.py:_build_session_title
 * PoP: acpsess_format_updated_at @ acp_adapter/session.py:_format_updated_at
 * PoP: acpsess_updated_at_sort_key @ acp_adapter/session.py:_updated_at_sort_key
 * PoP: acpsess_parse_model_config @ acp_adapter/session.py:_parse_model_config
 * PoP: acpsess_expand_acp_enabled_toolsets @ acp_adapter/session.py:_expand_acp_enabled_toolsets
 * PoP: acpsess_session_info @ acp_adapter/session.py:_session_info
 * PoP: acpsess_first_user_preview @ acp_adapter/session.py:_first_user_preview
 *
 * Agent construction, persistence and threading are deliberately out of scope;
 * only deterministic helpers used by ACP session listing are ported.
 */
#include "noc_w6_session_5810af.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *acpsess_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (out) memcpy(out, s, n + 1);
    return out;
}

/* Python str(value).strip() for the JSON scalar types used by session metadata. */
static char *acpsess_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return acpsess_dup("");
    if (v->type == JSON_STRING) return acpsess_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return acpsess_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15)
            snprintf(buf, sizeof(buf), "%.1f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return acpsess_dup(buf);
    }
    return acpsess_dup("");
}

static char *acpsess_trim(const char *s) {
    if (!s) return acpsess_dup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

static int acpsess_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return json_object_size(v) || json_len(v);
    return 0;
}

/* os.path.basename on either separator (ntpath and posixpath agree on '/'). */
static const char *acpsess_basename(const char *p) {
    if (!p || !*p) return "";
    const char *last = p;
    for (const char *c = p; *c; c++)
        if (*c == '/' || *c == '\\') last = c + 1;
    return last;
}

/* Days since 1970-01-01 (Howard Hinnant's civil calendar algorithm). */
static long long acpsess_days_from_civil(long long y, int m, int d) {
    y -= (m <= 2);
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

static json_t *acpsess_iso_from_epoch(double d) {
    if (!isfinite(d)) return json_null();
    long long sec = (long long)floor(d);
    long long micro = (long long)llround((d - (double)sec) * 1000000.0);
    if (micro >= 1000000) { micro -= 1000000; sec++; }
    time_t t = (time_t)sec;
    struct tm tmv;
#ifdef _WIN32
    if (gmtime_s(&tmv, &t) != 0) return json_null();
#else
    if (!gmtime_r(&t, &tmv)) return json_null();
#endif
    char buf[80];
    if (micro)
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%06lld+00:00",
                 tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                 tmv.tm_hour, tmv.tm_min, tmv.tm_sec, micro);
    else
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d+00:00",
                 tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                 tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return json_string(buf);
}

static int acpsess_parse_double(const char *s, double *out) {
    if (!s || !*s) return 0;
    char *end = NULL;
    double d = strtod(s, &end);
    if (!end || *end != '\0') return 0;
    *out = d;
    return 1;
}

/* datetime.fromisoformat(...) for the explicit-UTC forms fixtures use. */
static int acpsess_parse_iso(const char *s, double *out) {
    if (!s || strlen(s) < 19) return 0;
    if (s[4] != '-' || s[7] != '-' || (s[10] != 'T' && s[10] != ' ')) return 0;
    for (int i = 0; i < 19; i++) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) continue;
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    int y, mo, d, h, mi, se;
    y = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
    mo = (s[5]-'0')*10 + (s[6]-'0');
    d = (s[8]-'0')*10 + (s[9]-'0');
    h = (s[11]-'0')*10 + (s[12]-'0');
    mi = (s[14]-'0')*10 + (s[15]-'0');
    se = (s[17]-'0')*10 + (s[18]-'0');
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || se > 59) return 0;
    double frac = 0.0;
    const char *p = s + 19;
    if (*p == '.') {
        p++;
        if (!isdigit((unsigned char)*p)) return 0;
        double scale = 0.1;
        while (*p && isdigit((unsigned char)*p)) { frac += (*p - '0') * scale; scale *= 0.1; p++; }
    }
    if (!(*p == '\0' || !strcmp(p, "Z") || !strcmp(p, "+00:00") || !strcmp(p, "+0000"))) return 0;
    double epoch = (double)(acpsess_days_from_civil(y, mo, d) * 86400LL + h * 3600 + mi * 60 + se) + frac;
    *out = epoch;
    return 1;
}

json_t *acpsess_build_session_title(const json_t *title, const json_t *preview, const char *cwd) {
    char *t = acpsess_trim(acpsess_py_str(title));
    if (*t) { char *r = acpsess_dup(t); free(t); return json_string(r); }
    free(t);
    char *p = acpsess_trim(acpsess_py_str(preview));
    if (*p) { char *r = acpsess_dup(p); free(p); return json_string(r); }
    free(p);
    const char *raw = cwd ? cwd : "";
    size_t n = strlen(raw);
    while (n > 0 && (raw[n-1] == '/' || raw[n-1] == '\\')) n--;
    char *leafbuf = (char *)malloc(n + 1);
    if (leafbuf) { memcpy(leafbuf, raw, n); leafbuf[n] = '\0'; }
    const char *leaf = acpsess_basename(leafbuf ? leafbuf : "");
    json_t *out = json_string(*leaf ? leaf : "New thread");
    free(leafbuf);
    return out;
}

json_t *acpsess_format_updated_at(const json_t *value) {
    if (!value || value->type == JSON_NULL) return json_null();
    if (value->type == JSON_STRING) {
        char *t = acpsess_trim(value->str_val);
        int blank = (*t == '\0');
        free(t);
        if (!blank) return json_copy(value);
        double d;
        if (!acpsess_parse_double(value->str_val ? value->str_val : "", &d)) return json_null();
        return acpsess_iso_from_epoch(d);
    }
    if (value->type == JSON_NUMBER) return acpsess_iso_from_epoch(value->num_val);
    return json_null();
}

json_t *acpsess_updated_at_sort_key(const json_t *value) {
    if (value && value->type == JSON_NUMBER) return json_copy(value);
    char *raw = acpsess_trim(acpsess_py_str(value));
    if (!*raw) { free(raw); return json_string("-inf"); }
    double d;
    if (acpsess_parse_iso(raw, &d) || acpsess_parse_double(raw, &d)) {
        free(raw);
        return json_number(d);
    }
    free(raw);
    return json_string("-inf");
}

json_t *acpsess_parse_model_config(const json_t *mc) {
    if (mc && mc->type == JSON_STRING && mc->str_val && mc->str_val[0]) {
        json_t *parsed = json_parse(mc->str_val, NULL);
        if (parsed && parsed->type == JSON_OBJECT) return parsed;
        if (parsed) json_free(parsed);
    }
    return json_object();
}

static void acpsess_append_unique(json_t *arr, const char *s) {
    if (!s || !*s) return;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, s)) return;
    }
    json_append(arr, json_string(s));
}

json_t *acpsess_expand_acp_enabled_toolsets(const json_t *toolsets, const json_t *mcp_names) {
    json_t *out = json_array();
    if (!toolsets || toolsets->type == JSON_NULL) {
        acpsess_append_unique(out, "hermes-acp");
    } else if (toolsets->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(toolsets); i++) {
            json_t *e = json_get(toolsets, i);
            if (e && e->type == JSON_STRING && e->str_val && e->str_val[0])
                acpsess_append_unique(out, e->str_val);
        }
    }
    if (mcp_names && mcp_names->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(mcp_names); i++) {
            json_t *e = json_get(mcp_names, i);
            if (!e || e->type != JSON_STRING || !e->str_val || !e->str_val[0]) continue;
            size_t n = strlen(e->str_val) + 5;
            char *name = (char *)malloc(n);
            if (name) { snprintf(name, n, "mcp-%s", e->str_val); acpsess_append_unique(out, name); free(name); }
        }
    }
    return out;
}

json_t *acpsess_session_info(const char *sid, const char *cwd, const json_t *model,
                             double history_len, const json_t *title, const json_t *preview,
                             const json_t *updated_at) {
    json_t *out = json_object();
    json_set(out, "session_id", json_string(sid ? sid : ""));
    json_set(out, "cwd", json_string(cwd ? cwd : ""));
    json_set(out, "model", model ? json_copy(model) : json_null());
    json_set(out, "history_len", json_number(history_len));
    json_set(out, "title", acpsess_build_session_title(title, preview, cwd));
    json_set(out, "updated_at", acpsess_format_updated_at(updated_at));
    return out;
}

json_t *acpsess_first_user_preview(const json_t *history, const char *default_val) {
    if (history && history->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(history); i++) {
            json_t *m = json_get(history, i);
            if (!m || m->type != JSON_OBJECT) continue;
            json_t *role = json_obj_get(m, "role");
            if (!role || role->type != JSON_STRING || !role->str_val || strcmp(role->str_val, "user")) continue;
            char *content = acpsess_trim(acpsess_py_str(json_obj_get(m, "content")));
            if (*content) { char *r = acpsess_dup(content); free(content); return json_string(r); }
            free(content);
        }
    }
    return json_string(default_val ? default_val : "");
}
