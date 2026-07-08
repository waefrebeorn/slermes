/*
 * port_learning_graph_helpers.c
 *
 * Pure, portable helpers ported from agent/learning_graph.py.
 *
 * These six are self-contained (no filesystem / network / SkillNode dataclass):
 *   - _hermes_meta(fm)        -> metadata.hermes dict (tolerant of string fm)
 *   - _related(fm)            -> list of related skill names
 *   - _category(fm, path)     -> category or path-derived fallback
 *   - _to_int_ts(value)       -> int epoch (from int/float/numeric/ISO string)
 *   - _usage_timestamp(rec)   -> first non-null activity timestamp field
 *   - _tokenize(text)         -> set of tokens len>=3 (returned as JSON array)
 *
 * The two struct-coupled functions (density_stats, build_edges) are left as
 * honest REAL_GAP: they require reconstructing the SkillNode dataclass, which
 * is out of scope for a pure-helper port.
 *
 * Module prefix used by the scanner for agent/learning_graph.py is
 * "learning_graph_".
 *
 * C name <- python name (learning_graph_ prefix):
 *   hermes_meta, related, category, to_int_ts, usage_timestamp, tokenize
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "hermes_json.h"

/* --- ISO-8601 -> time_t (UTC) ----------------------------------------- */
static int parse_iso_to_ts(const char *value, time_t *out)
{
    if (!value || !*value) return 0;
    int Y, M, D, h = 0, m = 0, s = 0;
    if (sscanf(value, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) < 3)
        return 0;
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = Y - 1900;
    tmv.tm_mon = M - 1;
    tmv.tm_mday = D;
    tmv.tm_hour = h;
    tmv.tm_min = m;
    tmv.tm_sec = s;
    tmv.tm_isdst = 0;
#ifdef _WIN32
    *out = _mkgmtime(&tmv);
#else
    *out = timegm(&tmv);
#endif
    return (*out != (time_t)-1) ? 1 : 0;
}

/* ---------------------------------------------------------------------- */
/* PoP: _hermes_meta @ agent/learning_graph.py:_hermes_meta */
/* fm_json: JSON object. Returns malloc'd JSON of metadata.hermes (or "{}"). */
char *learning_graph_hermes_meta(const char *fm_json)
{
    json_t *fm = json_parse(fm_json ? fm_json : "{}", NULL);
    if (!fm || fm->type != JSON_OBJECT) { if (fm) json_free(fm); return strdup("{}"); }
    json_t *meta = json_object_get(fm, "metadata");
    json_t *hermes = (meta && meta->type == JSON_OBJECT) ? json_object_get(meta, "hermes") : NULL;
    char *out;
    if (hermes && hermes->type == JSON_OBJECT) {
        char *dump = json_dumps(hermes, 0);
        out = strdup(dump ? dump : "{}");
        free(dump);
    } else {
        out = strdup("{}");
    }
    json_free(fm);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: _related @ agent/learning_graph.py:_related */
/* fm_json: JSON object. Returns malloc'd JSON array of related skill names. */
char *learning_graph_related(const char *fm_json)
{
    json_t *fm = json_parse(fm_json ? fm_json : "{}", NULL);
    if (!fm || fm->type != JSON_OBJECT) { if (fm) json_free(fm); return strdup("[]"); }

    json_t *raw = json_object_get(fm, "related_skills");
    if (!raw) {
        /* fall back to metadata.hermes.related_skills */
        json_t *meta = json_object_get(fm, "metadata");
        json_t *hermes = (meta && meta->type == JSON_OBJECT) ? json_object_get(meta, "hermes") : NULL;
        if (hermes && hermes->type == JSON_OBJECT)
            raw = json_object_get(hermes, "related_skills");
    }

    json_t *arr = json_new_array();
    if (raw && raw->type == JSON_ARRAY) {
        size_t n = json_array_size(raw);
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_array_get(raw, i);
            if (e && e->type == JSON_STRING) {
                const char *sv = json_string_value(e);
                char *cp = strdup(sv ? sv : "");
                char *p = cp;
                while (*p) { *p = (char)tolower((unsigned char)*p); if (*p == ' ') *p = '\0'; p++; }
                if (cp[0]) json_array_append(arr, json_string(cp));
                free(cp);
            }
        }
    } else if (raw && raw->type == JSON_STRING) {
        /* "a, b, c" or "[a, b, c]" -> split on comma */
        const char *sv = json_string_value(raw);
        char *buf = strdup(sv ? sv : "");
        /* strip surrounding brackets */
        char *b = buf;
        if (*b == '[') b++;
        size_t bl = strlen(b);
        if (bl > 0 && b[bl-1] == ']') b[bl-1] = '\0';
        char *tok = strtok(b, ",");
        while (tok) {
            /* trim */
            while (*tok == ' ') tok++;
            char *end = tok + strlen(tok);
            while (end > tok && end[-1] == ' ') end--;
            *end = '\0';
            if (*tok) json_array_append(arr, json_string(tok));
            tok = strtok(NULL, ",");
        }
        free(buf);
    }
    char *out = json_dumps(arr, 0);
    char *ret = strdup(out ? out : "[]");
    free(out);
    json_free(arr);
    json_free(fm);
    return ret;
}

/* ---------------------------------------------------------------------- */
/* PoP: _category @ agent/learning_graph.py:_category */
/* fm_json: JSON object; fallback: caller-computed path-derived category
 * (mirrors skill_md.parts[-3]). Returns malloc'd category string. */
char *learning_graph_category(const char *fm_json, const char *fallback)
{
    json_t *fm = json_parse(fm_json ? fm_json : "{}", NULL);
    if (!fm || fm->type != JSON_OBJECT) { if (fm) json_free(fm); return strdup(fallback ? fallback : "general"); }
    json_t *cat = json_object_get(fm, "category");
    if (!cat || cat->type != JSON_STRING || !json_string_value(cat)[0]) {
        json_t *meta = json_object_get(fm, "metadata");
        json_t *hermes = (meta && meta->type == JSON_OBJECT) ? json_object_get(meta, "hermes") : NULL;
        if (hermes && hermes->type == JSON_OBJECT) {
            json_t *hc = json_object_get(hermes, "category");
            if (hc && hc->type == JSON_STRING && json_string_value(hc)[0]) cat = hc;
        }
    }
    char *out;
    if (cat && cat->type == JSON_STRING && json_string_value(cat)[0])
        out = strdup(json_string_value(cat));
    else
        out = strdup(fallback ? fallback : "general");
    json_free(fm);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: _to_int_ts @ agent/learning_graph.py:_to_int_ts */
/* value_json: a JSON scalar (number or string). *found set to 1 on success.
 * Returns epoch seconds (or 0 with *found=0 on failure). */
long long learning_graph_to_int_ts(const char *value_json, int *found)
{
    *found = 0;
    json_t *v = json_parse(value_json ? value_json : "null", NULL);
    if (!v) return 0;
    long long result = 0;
    if (v->type == JSON_NUMBER) {
        result = (long long)json_number_value(v);
        *found = 1;
    } else if (v->type == JSON_STRING) {
        const char *s = json_string_value(v);
        if (s && *s) {
            char *end = NULL;
            double d = strtod(s, &end);
            if (end != s && *end == '\0') {
                result = (long long)d;
                *found = 1;
            } else {
                /* ISO string */
                time_t t;
                if (parse_iso_to_ts(s, &t)) { result = (long long)t; *found = 1; }
            }
        }
    }
    json_free(v);
    return result;
}

/* ---------------------------------------------------------------------- */
/* PoP: _usage_timestamp @ agent/learning_graph.py:_usage_timestamp */
/* rec_json: JSON object. Returns first valid ts among the priority keys,
 * or 0 with *found=0 if none. */
long long learning_graph_usage_timestamp(const char *rec_json, int *found)
{
    static const char *keys[] = {"last_activity_at", "last_used_at", "last_viewed_at",
                                 "last_patched_at", "created_at"};
    *found = 0;
    json_t *rec = json_parse(rec_json ? rec_json : "{}", NULL);
    if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); return 0; }
    long long best = 0;
    for (int i = 0; i < 5; i++) {
        json_t *kv = json_object_get(rec, keys[i]);
        if (!kv) continue;
        char *sv = json_dumps(kv, 0);
        int f = 0;
        long long ts = learning_graph_to_int_ts(sv, &f);
        free(sv);
        if (f) { best = ts; *found = 1; break; }
    }
    json_free(rec);
    return best;
}

/* ---------------------------------------------------------------------- */
/* PoP: _tokenize @ agent/learning_graph.py:_tokenize */
/* Returns malloc'd JSON array of tokens (len>=3) from text. */
char *learning_graph_tokenize(const char *text)
{
    json_t *arr = json_new_array();
    if (text && *text) {
        char *buf = strdup(text);
        for (char *p = buf; *p; p++) {
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
        }
        /* split on runs of non [a-z0-9] */
        char *start = buf;
        char *p = buf;
        while (1) {
            int isword = (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9');
            if (!isword || *p == '\0') {
                if (p > start) {
                    char saved = *p;
                    *p = '\0';
                    size_t len = strlen(start);
                    if (len >= 3) json_array_append(arr, json_string(start));
                    *p = saved;
                }
                start = p + 1;
                if (*p == '\0') break;
            }
            p++;
        }
        free(buf);
    }
    char *out = json_dumps(arr, 0);
    char *ret = strdup(out ? out : "[]");
    free(out);
    json_free(arr);
    return ret;
}
