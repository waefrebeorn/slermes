/*
 * catalog.c — real C11 port of the pure catalog projection of upstream Hermes
 * acp_adapter/catalog.py (gate w6_catalog_c1c3fb).
 *
 * PoP: acpcat_catalog_sessions @ acp_adapter/catalog.py:catalog_sessions
 * PoP: acpcat_format_updated_at @ acp_adapter/catalog.py:_format_updated_at
 * PoP: acpcat_updated_at_sort_key @ acp_adapter/catalog.py:_updated_at_sort_key
 *
 * The store is injected as JSON (rows + owners); the decision layer — lineage
 * root, logical owner, message-count filter, model_config cwd fallback,
 * per-logical-session newest-segment selection and descending sort — is ported
 * verbatim.
 */
#include "noc_w6_catalog_c1c3fb.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *acpcat_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (out) memcpy(out, s, n + 1);
    return out;
}

static char *acpcat_trim(const char *s) {
    if (!s) return acpcat_dup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

static int acpcat_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY) return json_len(v) != 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) != 0;
    return 0;
}

static char *acpcat_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return acpcat_dup("");
    if (v->type == JSON_STRING) return acpcat_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return acpcat_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return acpcat_dup(buf);
    }
    return acpcat_dup("");
}

static const char *acpcat_basename(const char *p) {
    if (!p || !*p) return "";
    const char *last = p;
    for (const char *c = p; *c; c++)
        if (*c == '/' || *c == '\\') last = c + 1;
    return last;
}

static long long acpcat_days_from_civil(long long y, int m, int d) {
    y -= (m <= 2);
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

static json_t *acpcat_iso_from_epoch(double d) {
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

static int acpcat_parse_double(const char *s, double *out) {
    if (!s || !*s) return 0;
    char *end = NULL;
    double d = strtod(s, &end);
    if (!end || *end != '\0') return 0;
    *out = d;
    return 1;
}

static int acpcat_parse_iso(const char *s, double *out) {
    if (!s || strlen(s) < 19) return 0;
    if (s[4] != '-' || s[7] != '-' || (s[10] != 'T' && s[10] != ' ')) return 0;
    for (int i = 0; i < 19; i++) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) continue;
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    int y = (s[0]-'0')*1000+(s[1]-'0')*100+(s[2]-'0')*10+(s[3]-'0');
    int mo = (s[5]-'0')*10+(s[6]-'0'), d = (s[8]-'0')*10+(s[9]-'0');
    int h = (s[11]-'0')*10+(s[12]-'0'), mi = (s[14]-'0')*10+(s[15]-'0');
    int se = (s[17]-'0')*10+(s[18]-'0');
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
    *out = (double)(acpcat_days_from_civil(y, mo, d) * 86400LL + h * 3600 + mi * 60 + se) + frac;
    return 1;
}

json_t *acpcat_format_updated_at(const json_t *value) {
    if (!value || value->type == JSON_NULL) return json_null();
    if (value->type == JSON_STRING) {
        char *t = acpcat_trim(value->str_val);
        int blank = (*t == '\0');
        free(t);
        if (!blank) return json_copy(value);
        double d;
        if (!acpcat_parse_double(value->str_val ? value->str_val : "", &d)) return json_null();
        return acpcat_iso_from_epoch(d);
    }
    if (value->type == JSON_NUMBER) return acpcat_iso_from_epoch(value->num_val);
    return json_null();
}

json_t *acpcat_updated_at_sort_key(const json_t *value) {
    if (value && value->type == JSON_NUMBER) return json_copy(value);
    char *raw = acpcat_trim(acpcat_str(value));
    if (!*raw) { free(raw); return json_string("-inf"); }
    double d;
    if (acpcat_parse_iso(raw, &d) || acpcat_parse_double(raw, &d)) { free(raw); return json_number(d); }
    free(raw);
    return json_string("-inf");
}

static double acpcat_key(const json_t *value) {
    if (value && value->type == JSON_NUMBER) return value->num_val;
    char *raw = acpcat_trim(acpcat_str(value));
    double d;
    if (*raw && (acpcat_parse_iso(raw, &d) || acpcat_parse_double(raw, &d))) { free(raw); return d; }
    free(raw);
    return -INFINITY;
}

static json_t *acpcat_build_title(const json_t *title, const json_t *preview, const char *cwd) {
    char *t = acpcat_trim(acpcat_str(title));
    if (*t) { char *r = acpcat_dup(t); free(t); return json_string(r); }
    free(t);
    char *p = acpcat_trim(acpcat_str(preview));
    if (*p) { char *r = acpcat_dup(p); free(p); return json_string(r); }
    free(p);
    const char *raw = cwd ? cwd : "";
    size_t n = strlen(raw);
    while (n > 0 && (raw[n-1] == '/' || raw[n-1] == '\\')) n--;
    char *buf = (char *)malloc(n + 1);
    if (buf) { memcpy(buf, raw, n); buf[n] = '\0'; }
    const char *leaf = acpcat_basename(buf ? buf : "");
    json_t *out = json_string(*leaf ? leaf : "New thread");
    free(buf);
    return out;
}

static long long acpcat_py_int(const json_t *v, long long def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        while (*s && isspace((unsigned char)*s)) s++;
        int sign = 1;
        if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; s++; }
        if (!*s || !isdigit((unsigned char)*s)) return def;
        long long acc = 0;
        while (*s && isdigit((unsigned char)*s)) { acc = acc * 10 + (*s - '0'); s++; }
        while (*s && isspace((unsigned char)*s)) s++;
        if (*s) return def;
        return sign * acc;
    }
    return def;
}

typedef struct {
    char *sid;
    json_t *info;
    double key;
} acpcat_row_t;

json_t *acpcat_catalog_sessions(const json_t *rows, const json_t *owners) {
    acpcat_row_t result[4096];
    size_t count = 0;
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rows) && count < 4096; i++) {
            json_t *row = json_get(rows, i);
            if (!row || row->type != JSON_OBJECT) continue;
            char *sid = acpcat_str(json_obj_get(row, "id"));
            json_t *lr = json_obj_get(row, "_lineage_root_id");
            if (acpcat_truthy(lr)) { free(sid); sid = acpcat_str(lr); }
            json_t *owner = owners && owners->type == JSON_OBJECT ? json_obj_get(owners, sid) : NULL;
            if (owner && owner->type == JSON_STRING && owner->str_val) { free(sid); sid = acpcat_dup(owner->str_val); }
            json_t *mc_node = json_obj_get(row, "message_count");
            long long n = acpcat_truthy(mc_node) ? acpcat_py_int(mc_node, 0) : 0;
            if (n <= 0) { free(sid); continue; }
            char *cwd = acpcat_str(json_obj_get(row, "cwd"));
            if (!*cwd) {
                /* _parse_model_config(...).get("cwd", ".") — default "." even
                 * when model_config is absent, null, invalid or non-dict. */
                const char *from_cfg = ".";
                json_t *mc = json_obj_get(row, "model_config");
                json_t *parsed = NULL;
                if (mc && mc->type == JSON_STRING && mc->str_val && mc->str_val[0])
                    parsed = json_parse(mc->str_val, NULL);
                if (parsed && parsed->type == JSON_OBJECT) {
                    json_t *cv = json_obj_get(parsed, "cwd");
                    if (cv && cv->type == JSON_STRING && cv->str_val && cv->str_val[0])
                        from_cfg = cv->str_val;
                }
                free(cwd);
                cwd = acpcat_dup(from_cfg);
                if (parsed) json_free(parsed);
            }
            json_t *updated = json_obj_get(row, "last_active");
            if (!acpcat_truthy(updated)) updated = json_obj_get(row, "started_at");
            json_t *info = json_object();
            json_set(info, "session_id", json_string(sid));
            json_set(info, "cwd", json_string(cwd ? cwd : ""));
            json_t *model = json_obj_get(row, "model");
            if (model && acpcat_truthy(model)) {
                char *m = acpcat_str(model);
                json_set(info, "model", json_string(m));
                free(m);
            } else {
                json_set(info, "model", json_string(""));
            }
            json_set(info, "history_len", json_number((double)n));
            json_set(info, "title", acpcat_build_title(json_obj_get(row, "title"),
                                                        json_obj_get(row, "preview"), cwd));
            json_set(info, "updated_at", acpcat_format_updated_at(updated));
            double key = acpcat_key(json_obj_get(info, "updated_at"));
            int found = -1;
            for (size_t j = 0; j < count; j++)
                if (!strcmp(result[j].sid, sid)) { found = (int)j; break; }
            if (found >= 0) {
                if (key > result[found].key) {
                    json_free(result[found].info);
                    result[found].info = info;
                    result[found].key = key;
                    free(sid);
                } else {
                    json_free(info);
                    free(sid);
                }
                free(cwd);
                continue;
            }
            result[count].sid = sid;
            result[count].info = info;
            result[count].key = key;
            count++;
            free(cwd);
        }
    }
    /* Stable insertion sort by key descending (mirrors sorted(..., reverse=True)). */
    for (size_t i = 1; i < count; i++) {
        acpcat_row_t cur = result[i];
        size_t j = i;
        while (j > 0 && cur.key > result[j-1].key) { result[j] = result[j-1]; j--; }
        result[j] = cur;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < count; i++) {
        json_append(out, result[i].info);
        free(result[i].sid);
    }
    return out;
}
