/*
 * models.c -- C11 port of the normalized models of the Teams meeting pipeline
 * plugin, upstream plugins/teams_pipeline/models.py.
 *
 * PoP: w20_models_59cc22_camel @ plugins/teams_pipeline/models.py:_camel
 * PoP: w20_models_59cc22_str @ plugins/teams_pipeline/models.py:_str
 * PoP: w20_models_59cc22_pick @ plugins/teams_pipeline/models.py:_pick
 * PoP: w20_models_59cc22_parse_datetime @ plugins/teams_pipeline/models.py:_parse_datetime
 * PoP: w20_models_59cc22_from_dict @ plugins/teams_pipeline/models.py:_Model.from_dict
 * PoP: w20_models_59cc22_to_dict @ plugins/teams_pipeline/models.py:_Model.to_dict
 * PoP: w20_models_59cc22_roundtrip @ plugins/teams_pipeline/models.py:_Model.to_dict
 *
 * The module imports only the standard library, so nothing stays upstream: the
 * five dataclasses, their _ALIASES/_CONVERT/_REQUIRED/_DATETIMES tables and the
 * post-init validation are all reproduced. A datetime field is carried in its
 * UTC `...Z` wire spelling, which is all to_dict ever emits for one.
 */
#include "hermes_json.h"
#include "noc_w20_models_59cc22.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *tm_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python truthiness; absent (NULL) reads as None. */
static int tm_truthy(const json_t *v) {
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

static int tm_is_int_number(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d)) return 0;
    return floor(d) == d && d >= -9.007199254740992e15 && d <= 9.007199254740992e15;
}

/* --- small text builder ---------------------------------------------------- */

typedef struct { char *p; size_t n, cap; } tm_buf;

static int tm_buf_add(tm_buf *b, const char *s) {
    size_t n = s ? strlen(s) : 0;
    if (!n) return 1;
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *np;
        while (b->n + n + 1 > cap) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return 0;
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
    return 1;
}

static int tm_buf_ch(tm_buf *b, char c) {
    char s[2];
    s[0] = c;
    s[1] = '\0';
    return tm_buf_add(b, s);
}

static char *tm_buf_take(tm_buf *b) {
    if (!b->p) return tm_dup("");
    return b->p;
}

/* --- Python str()/repr() of a decoded JSON value --------------------------- */

/* Shortest %g spelling that round-trips, matching float.__repr__ closely. */
static void tm_fmt_double(double d, char *out, size_t cap) {
    int prec;
    if (!isfinite(d)) {
        snprintf(out, cap, "%s", isnan(d) ? "nan" : (d < 0 ? "-inf" : "inf"));
        return;
    }
    for (prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, d);
        if (strtod(out, NULL) == d) break;
    }
}

static int tm_repr_into(tm_buf *b, const json_t *v);

static int tm_repr_str_into(tm_buf *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    if (!tm_buf_ch(b, '\'')) return 0;
    for (; *p; p++) {
        char esc[8];
        switch (*p) {
        case '\\': if (!tm_buf_add(b, "\\\\")) return 0; break;
        case '\'': if (!tm_buf_add(b, "\\'")) return 0; break;
        case '\n': if (!tm_buf_add(b, "\\n")) return 0; break;
        case '\r': if (!tm_buf_add(b, "\\r")) return 0; break;
        case '\t': if (!tm_buf_add(b, "\\t")) return 0; break;
        default:
            if (*p < 0x20) {
                snprintf(esc, sizeof(esc), "\\x%02x", (unsigned)*p);
                if (!tm_buf_add(b, esc)) return 0;
            } else if (!tm_buf_ch(b, (char)*p)) {
                return 0;
            }
        }
    }
    return tm_buf_ch(b, '\'');
}

static int tm_repr_into(tm_buf *b, const json_t *v) {
    size_t i;
    if (!v) return tm_buf_add(b, "None");
    switch (v->type) {
    case JSON_NULL: return tm_buf_add(b, "None");
    case JSON_BOOL: return tm_buf_add(b, v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        char tmp[64];
        if (tm_is_int_number(v)) snprintf(tmp, sizeof(tmp), "%lld", (long long)v->num_val);
        else tm_fmt_double(v->num_val, tmp, sizeof(tmp));
        return tm_buf_add(b, tmp);
    }
    case JSON_STRING: return tm_repr_str_into(b, v->str_val);
    case JSON_ARRAY:
        if (!tm_buf_ch(b, '[')) return 0;
        for (i = 0; i < v->c.count; i++) {
            if (i && !tm_buf_add(b, ", ")) return 0;
            if (!tm_repr_into(b, v->c.items[i])) return 0;
        }
        return tm_buf_ch(b, ']');
    case JSON_OBJECT:
        if (!tm_buf_ch(b, '{')) return 0;
        for (i = 0; i < v->c.count; i++) {
            if (i && !tm_buf_add(b, ", ")) return 0;
            if (!tm_repr_str_into(b, v->c.keys[i])) return 0;
            if (!tm_buf_add(b, ": ")) return 0;
            if (!tm_repr_into(b, v->c.items[i])) return 0;
        }
        return tm_buf_ch(b, '}');
    }
    return 0;
}

static char *tm_str(const json_t *v) {
    tm_buf b;
    char tmp[64];
    memset(&b, 0, sizeof(b));
    if (!v || v->type == JSON_NULL) return tm_dup("None");
    if (v->type == JSON_STRING) return tm_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return tm_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (tm_is_int_number(v)) snprintf(tmp, sizeof(tmp), "%lld", (long long)v->num_val);
        else tm_fmt_double(v->num_val, tmp, sizeof(tmp));
        return tm_dup(tmp);
    }
    if (!tm_repr_into(&b, v)) { free(b.p); return tm_dup(""); }
    return tm_buf_take(&b);
}

/* Python str.strip() over ASCII whitespace. */
static void tm_strip_inplace(char *s) {
    char *start = s;
    size_t n;
    if (!s) return;
    while (*start && (unsigned char)*start <= 0x20) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    n = strlen(s);
    while (n && (unsigned char)s[n - 1] <= 0x20) s[--n] = '\0';
}

/* --- _camel --------------------------------------------------------------- */

/* str.title(): upper-case the first alpha of every run, lower-case the rest. */
static char *tm_title(const char *part) {
    char *out = tm_dup(part ? part : "");
    int prev_alpha = 0;
    size_t i;
    if (!out) return NULL;
    for (i = 0; out[i]; i++) {
        unsigned char c = (unsigned char)out[i];
        if (isalpha(c)) {
            out[i] = (char)(prev_alpha ? tolower(c) : toupper(c));
            prev_alpha = 1;
        } else {
            prev_alpha = 0;
        }
    }
    return out;
}

char *w20_models_59cc22_camel(const char *name) {
    tm_buf b;
    const char *p, *seg;
    int first = 1;
    memset(&b, 0, sizeof(b));
    if (!name) name = "";
    p = name;
    seg = p;
    for (;;) {
        if (*p == '_' || *p == '\0') {
            size_t len = (size_t)(p - seg);
            char *part = (char *)malloc(len + 1);
            char *titled;
            if (!part) { free(b.p); return NULL; }
            memcpy(part, seg, len);
            part[len] = '\0';
            titled = first ? tm_dup(part) : tm_title(part);
            free(part);
            if (!titled) { free(b.p); return NULL; }
            if (!tm_buf_add(&b, titled)) { free(titled); free(b.p); return NULL; }
            free(titled);
            first = 0;
            if (*p == '\0') break;
            seg = p + 1;
        }
        p++;
    }
    return tm_buf_take(&b);
}

char *w20_models_59cc22_str(const json_t *value) {
    char *out;
    if (!tm_truthy(value)) return tm_dup("");
    out = tm_str(value);
    if (!out) return NULL;
    tm_strip_inplace(out);
    return out;
}

json_t *w20_models_59cc22_pick(const json_t *payload, const char *const *keys, size_t nkeys) {
    json_t *last = NULL;
    size_t i;
    if (!keys || nkeys == 0) return json_null();
    for (i = 0; i < nkeys; i++) {
        json_t *v = json_obj_get(payload, keys[i]);
        if (tm_truthy(v)) return v ? json_copy(v) : json_null();
    }
    last = json_obj_get(payload, keys[nkeys - 1]);
    return last ? json_copy(last) : json_null();
}

/* --- ISO-8601 datetime ----------------------------------------------------- */

static long long tm_days_from_civil(long long y, unsigned m, unsigned d) {
    long long era, yoe, doy, doe;
    unsigned mp = (unsigned)(m + (m > 2 ? -3 : 9));
    y -= (m <= 2) ? 1 : 0;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (long long)((153u * mp + 2u) / 5u + d - 1u);
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void tm_civil_from_days(long long z, long long *yy, unsigned *mm, unsigned *dd) {
    long long era, yoe, y, doy;
    unsigned doe, doy_u, mp;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (unsigned)(z - era * 146097);
    yoe = (long long)((doe - doe / 1460 + doe / 36524 - doe / 146096) / 365);
    y = yoe + era * 400;
    doy = (long long)doe - (365 * yoe + yoe / 4 - yoe / 100);
    doy_u = (unsigned)doy;
    mp = (5u * doy_u + 2u) / 153u;
    *dd = doy_u - (153u * mp + 2u) / 5u + 1u;
    *mm = mp + (mp < 10 ? 3u : (unsigned)-9);
    *yy = y + (*mm <= 2 ? 1 : 0);
}

static int tm_read_int(const char **pp, int digits, int *out) {
    const char *p = *pp;
    int v = 0, i;
    for (i = 0; i < digits; i++) {
        if (!isdigit((unsigned char)p[i])) return 0;
        v = v * 10 + (p[i] - '0');
    }
    *pp = p + digits;
    *out = v;
    return 1;
}

static int tm_days_in_month(long long y, unsigned m) {
    static const int DIM[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) return 0;
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return DIM[m];
}

/* Parse ISO-8601 to UTC epoch seconds + microseconds. 0 on failure. */
static int tm_parse_iso(const char *text, long long *secs, long *micro) {
    const char *p = text;
    int year = 0, mon = 0, day = 0, hh = 0, mi = 0, ss = 0, frac = 0;
    long long days, total;
    int off_sign = 1, off_h = 0, off_m = 0;
    if (!text) return 0;
    if (!tm_read_int(&p, 4, &year)) return 0;
    if (*p++ != '-') return 0;
    if (!tm_read_int(&p, 2, &mon)) return 0;
    if (*p++ != '-') return 0;
    if (!tm_read_int(&p, 2, &day)) return 0;
    if (year < 1 || year > 9999) return 0;
    if (mon < 1 || mon > 12) return 0;
    if (day < 1 || day > tm_days_in_month((long long)year, (unsigned)mon)) return 0;
    if (*p == 'T' || *p == ' ') {
        p++;
        if (!tm_read_int(&p, 2, &hh)) return 0;
        if (*p++ != ':') return 0;
        if (!tm_read_int(&p, 2, &mi)) return 0;
        if (*p == ':') {
            p++;
            if (!tm_read_int(&p, 2, &ss)) return 0;
        }
        if (*p == '.' || *p == ',') {
            int digits = 0, scale = 100000;
            p++;
            while (isdigit((unsigned char)*p) && digits < 6) {
                frac += (*p - '0') * scale;
                scale /= 10;
                digits++;
                p++;
            }
            if (digits == 0) return 0;
            while (isdigit((unsigned char)*p)) p++;
        }
    }
    if (*p == 'Z' || *p == 'z') {
        p++;
    } else if (*p == '+' || *p == '-') {
        off_sign = (*p == '-') ? -1 : 1;
        p++;
        if (!tm_read_int(&p, 2, &off_h)) return 0;
        if (*p == ':') {
            p++;
            if (!tm_read_int(&p, 2, &off_m)) return 0;
        } else if (isdigit((unsigned char)*p)) {
            if (!tm_read_int(&p, 2, &off_m)) return 0;
        }
        if (off_h > 23 || off_m > 59) return 0;
    }
    if (*p != '\0') return 0;
    if (hh > 23 || mi > 59 || ss > 59) return 0;
    days = tm_days_from_civil((long long)year, (unsigned)mon, (unsigned)day);
    total = days * 86400LL + hh * 3600LL + mi * 60LL + ss;
    total -= (long long)off_sign * (off_h * 3600LL + off_m * 60LL);
    *secs = total;
    *micro = frac;
    return 1;
}

static void tm_format_utc(long long secs, long micro, char *out, size_t cap) {
    long long days = secs / 86400, rem = secs % 86400, y;
    unsigned m, d;
    int hh, mi, ss;
    if (rem < 0) { rem += 86400; days -= 1; }
    tm_civil_from_days(days, &y, &m, &d);
    hh = (int)(rem / 3600);
    mi = (int)((rem % 3600) / 60);
    ss = (int)(rem % 60);
    if (micro) {
        snprintf(out, cap, "%04lld-%02u-%02uT%02d:%02d:%02d.%06ldZ", y, m, d, hh, mi, ss, (long)micro);
    } else {
        snprintf(out, cap, "%04lld-%02u-%02uT%02d:%02d:%02dZ", y, m, d, hh, mi, ss);
    }
}

/* _parse_datetime + _serialize_datetime for one raw JSON value. */
static char *tm_datetime_wire(const json_t *v, int *ok) {
    char *text, buf[48];
    long long secs;
    long micro;
    *ok = 1;
    if (!v || v->type == JSON_NULL) return NULL;
    text = tm_str(v);
    if (!text) { *ok = 0; return NULL; }
    tm_strip_inplace(text);
    if (!text[0]) { free(text); return NULL; }
    if (!tm_parse_iso(text, &secs, &micro)) {
        free(text);
        *ok = 0;
        return NULL;
    }
    free(text);
    tm_format_utc(secs, micro, buf, sizeof(buf));
    return tm_dup(buf);
}

json_t *w20_models_59cc22_parse_datetime(const char *value) {
    json_t *out = json_object();
    json_t *v = value ? json_string(value) : json_null();
    char *wire;
    int ok = 0;
    wire = tm_datetime_wire(v, &ok);
    json_free(v);
    if (ok) {
        json_set(out, "status", json_string("ok"));
        json_set(out, "value", wire ? json_string(wire) : json_null());
    } else {
        char msg[512];
        snprintf(msg, sizeof(msg), "Invalid isoformat string: '%.400s'", value ? value : "None");
        json_set(out, "status", json_string("error"));
        json_set(out, "type", json_string("ValueError"));
        json_set(out, "message", json_string(msg));
    }
    free(wire);
    return out;
}

/* --- model tables ---------------------------------------------------------- */

typedef enum {
    TMC_NONE = 0,
    TMC_STR,
    TMC_LIST,
    TMC_DICT,
    TMC_NESTED_REF,
    TMC_LIST_ARTIFACT
} tm_conv;

typedef struct {
    const char *name;
    tm_conv conv;
    int required;
    int datetime;
    int coerce_int;
    const char *aliases[5];   /* NULL-terminated; {NULL} => (name, CamelName) */
} tm_field;

typedef struct {
    const char *name;
    const tm_field *fields;
    size_t nfields;
    int artifact_type_check;
} tm_model;

static const tm_field SUB_FIELDS[] = {
    {"subscription_id",     TMC_STR,  1, 0, 0, {"subscription_id", "id", NULL}},
    {"resource",            TMC_STR,  1, 0, 0, {NULL}},
    {"change_type",         TMC_STR,  1, 0, 0, {NULL}},
    {"notification_url",    TMC_STR,  1, 0, 0, {NULL}},
    {"expiration_datetime", TMC_NONE, 0, 1, 0, {"expiration_datetime", "expirationDateTime", NULL}},
    {"client_state",        TMC_NONE, 0, 0, 0, {NULL}},
    {"latest_renewal_at",   TMC_NONE, 0, 1, 0, {NULL}},
    {"status",              TMC_NONE, 0, 0, 0, {NULL}},
};

static const tm_field REF_FIELDS[] = {
    {"meeting_id",        TMC_STR,  1, 0, 0, {"meeting_id", "id", NULL}},
    {"organizer_user_id", TMC_NONE, 0, 0, 0, {NULL}},
    {"join_web_url",      TMC_NONE, 0, 0, 0, {NULL}},
    {"calendar_event_id", TMC_NONE, 0, 0, 0, {NULL}},
    {"thread_id",         TMC_NONE, 0, 0, 0, {NULL}},
    {"tenant_id",         TMC_NONE, 0, 0, 0, {NULL}},
    {"metadata",          TMC_DICT, 0, 0, 0, {NULL}},
};

static const tm_field ARTIFACT_FIELDS[] = {
    {"artifact_type", TMC_NONE, 0, 0, 0, {NULL}},
    {"artifact_id",   TMC_STR,  1, 0, 0, {"artifact_id", "id", NULL}},
    {"display_name",  TMC_NONE, 0, 0, 0, {"display_name", "displayName", "name", NULL}},
    {"content_type",  TMC_NONE, 0, 0, 0, {NULL}},
    {"source_url",    TMC_NONE, 0, 0, 0, {"source_url", "sourceUrl", "webUrl", NULL}},
    {"download_url",  TMC_NONE, 0, 0, 0, {"download_url", "downloadUrl",
                                          "@microsoft.graph.downloadUrl", NULL}},
    {"created_at",    TMC_NONE, 0, 1, 0, {"created_at", "createdDateTime", NULL}},
    {"available_at",  TMC_NONE, 0, 1, 0, {"available_at", "availableDateTime",
                                          "lastModifiedDateTime", NULL}},
    {"size_bytes",    TMC_NONE, 0, 0, 1, {"size_bytes", "size", NULL}},
    {"metadata",      TMC_DICT, 0, 0, 0, {NULL}},
};

static const tm_field SUMMARY_FIELDS[] = {
    {"meeting_ref",       TMC_NESTED_REF,    0, 0, 0, {"meeting_ref", NULL}},
    {"title",             TMC_NONE,          0, 0, 0, {NULL}},
    {"start_time",        TMC_NONE,          0, 1, 0, {NULL}},
    {"end_time",          TMC_NONE,          0, 1, 0, {NULL}},
    {"participants",      TMC_LIST,          0, 0, 0, {NULL}},
    {"transcript_text",   TMC_NONE,          0, 0, 0, {NULL}},
    {"summary",           TMC_NONE,          0, 0, 0, {NULL}},
    {"key_decisions",     TMC_LIST,          0, 0, 0, {NULL}},
    {"action_items",      TMC_LIST,          0, 0, 0, {NULL}},
    {"risks",             TMC_LIST,          0, 0, 0, {NULL}},
    {"call_metrics",      TMC_DICT,          0, 0, 0, {NULL}},
    {"source_artifacts",  TMC_LIST_ARTIFACT, 0, 0, 0, {"source_artifacts", NULL}},
    {"confidence",        TMC_NONE,          0, 0, 0, {NULL}},
    {"confidence_notes",  TMC_NONE,          0, 0, 0, {NULL}},
    {"notion_target",     TMC_NONE,          0, 0, 0, {NULL}},
    {"linear_target",     TMC_NONE,          0, 0, 0, {NULL}},
    {"teams_target",      TMC_NONE,          0, 0, 0, {NULL}},
};

static const tm_field JOB_FIELDS[] = {
    {"job_id",                     TMC_STR,        1, 0, 0, {NULL}},
    {"event_id",                   TMC_STR,        1, 0, 0, {NULL}},
    {"source_event_type",          TMC_STR,        1, 0, 0, {NULL}},
    {"dedupe_key",                 TMC_STR,        1, 0, 0, {NULL}},
    {"status",                     TMC_STR,        1, 0, 0, {NULL}},
    {"retry_count",                TMC_NONE,       0, 0, 1, {NULL}},
    {"created_at",                 TMC_NONE,       0, 1, 0, {NULL}},
    {"updated_at",                 TMC_NONE,       0, 1, 0, {NULL}},
    {"meeting_ref",                TMC_NESTED_REF, 0, 0, 0, {NULL}},
    {"selected_artifact_strategy", TMC_NONE,       0, 0, 0, {NULL}},
    {"summary_payload",            TMC_NESTED_REF, 0, 0, 0, {NULL}},
    {"error_info",                 TMC_DICT,       0, 0, 0, {NULL}},
};

static const tm_model MODELS[] = {
    {"GraphSubscription",          SUB_FIELDS,      sizeof(SUB_FIELDS) / sizeof(SUB_FIELDS[0]),      0},
    {"TeamsMeetingRef",            REF_FIELDS,      sizeof(REF_FIELDS) / sizeof(REF_FIELDS[0]),      0},
    {"MeetingArtifact",            ARTIFACT_FIELDS, sizeof(ARTIFACT_FIELDS) / sizeof(ARTIFACT_FIELDS[0]), 1},
    {"TeamsMeetingSummaryPayload", SUMMARY_FIELDS,  sizeof(SUMMARY_FIELDS) / sizeof(SUMMARY_FIELDS[0]), 0},
    {"TeamsMeetingPipelineJob",    JOB_FIELDS,      sizeof(JOB_FIELDS) / sizeof(JOB_FIELDS[0]),      0},
};

static const tm_model *tm_model_find(const char *name) {
    size_t i;
    if (!name) return NULL;
    for (i = 0; i < sizeof(MODELS) / sizeof(MODELS[0]); i++) {
        if (!strcmp(MODELS[i].name, name)) return &MODELS[i];
    }
    return NULL;
}

/* --- conversion plumbing --------------------------------------------------- */

typedef struct {
    int raised;
    char type[32];
    char message[512];
} tm_err;

static void tm_raise(tm_err *err, const char *type, const char *message) {
    if (err->raised) return;
    err->raised = 1;
    snprintf(err->type, sizeof(err->type), "%s", type ? type : "Exception");
    snprintf(err->message, sizeof(err->message), "%s", message ? message : "");
}

/* Python type name used by the iterable/int() error texts. */
static const char *tm_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return tm_is_int_number(v) ? "int" : "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    default:          return "object";
    }
}

/* list(value or []) */
static json_t *tm_to_list(const json_t *v, tm_err *err) {
    json_t *arr = json_array();
    size_t i;
    if (!tm_truthy(v)) return arr;
    switch (v->type) {
    case JSON_ARRAY:
        for (i = 0; i < v->c.count; i++) json_append(arr, json_copy(v->c.items[i]));
        return arr;
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        for (i = 0; s[i]; i++) {
            char ch[2];
            ch[0] = s[i];
            ch[1] = '\0';
            json_append(arr, json_string(ch));
        }
        return arr;
    }
    case JSON_OBJECT:
        for (i = 0; i < v->c.count; i++) json_append(arr, json_string(v->c.keys[i]));
        return arr;
    default: {
        char msg[128];
        snprintf(msg, sizeof(msg), "'%s' object is not iterable", tm_type_name(v));
        tm_raise(err, "TypeError", msg);
        json_free(arr);
        return NULL;
    }
    }
}

/* dict(value or {}) */
static json_t *tm_as_dict(const json_t *v, tm_err *err) {
    json_t *obj = json_object();
    size_t i;
    if (!tm_truthy(v)) return obj;
    if (v->type == JSON_OBJECT) {
        for (i = 0; i < v->c.count; i++) json_set(obj, v->c.keys[i], json_copy(v->c.items[i]));
        return obj;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s);
        char msg[160];
        char key[2], val[2];
        if (n == 2) {
            key[0] = s[0]; key[1] = '\0';
            val[0] = s[1]; val[1] = '\0';
            json_set(obj, key, json_string(val));
            return obj;
        }
        snprintf(msg, sizeof(msg),
                 "dictionary update sequence element #0 has length %zu; 2 is required", n);
        tm_raise(err, "ValueError", msg);
        json_free(obj);
        return NULL;
    }
    if (v->type == JSON_ARRAY) {
        for (i = 0; i < v->c.count; i++) {
            json_t *item = v->c.items[i];
            char msg[160];
            if (item && item->type == JSON_ARRAY && item->c.count == 2 &&
                item->c.items[0] && item->c.items[0]->type == JSON_STRING) {
                json_t *val = item->c.items[1];
                json_set(obj, item->c.items[0]->str_val, val ? json_copy(val) : json_null());
                continue;
            }
            snprintf(msg, sizeof(msg),
                     "dictionary update sequence element #%zu has length %zu; 2 is required",
                     i, (item && item->type == JSON_ARRAY) ? item->c.count : (size_t)0);
            tm_raise(err, "ValueError", msg);
            json_free(obj);
            return NULL;
        }
        return obj;
    }
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "'%s' object is not iterable", tm_type_name(v));
        tm_raise(err, "TypeError", msg);
        json_free(obj);
        return NULL;
    }
}

/* int(value) */
static int tm_to_int(const json_t *v, long long *out, tm_err *err) {
    if (!v) {
        tm_raise(err, "TypeError",
                 "int() argument must be a string, a bytes-like object or a real number, "
                 "not 'NoneType'");
        return 0;
    }
    switch (v->type) {
    case JSON_BOOL:   *out = v->bool_val ? 1 : 0; return 1;
    case JSON_NUMBER:
        if (!isfinite(v->num_val)) {
            char msg[160];
            snprintf(msg, sizeof(msg), "cannot convert float %s to integer",
                     v->num_val < 0 ? "-inf" : (isnan(v->num_val) ? "nan" : "inf"));
            tm_raise(err, "ValueError", msg);
            return 0;
        }
        *out = (long long)v->num_val;
        return 1;
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        char buf[64], msg[512];
        size_t n = 0;
        int neg = 0;
        const char *p = s;
        const char *q;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        while (*p) {
            if (*p == '_' && n) { p++; continue; }
            if (!isdigit((unsigned char)*p)) break;
            if (n + 1 >= sizeof(buf)) break;
            buf[n++] = *p++;
        }
        buf[n] = '\0';
        q = p;
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
        if (*q != '\0' || n == 0) {
            snprintf(msg, sizeof(msg), "invalid literal for int() with base 10: '%.400s'", s);
            tm_raise(err, "ValueError", msg);
            return 0;
        }
        *out = strtoll(buf, NULL, 10);
        if (neg) *out = -*out;
        return 1;
    }
    default: {
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "int() argument must be a string, a bytes-like object or a real number, "
                 "not '%s'", tm_type_name(v));
        tm_raise(err, "TypeError", msg);
        return 0;
    }
    }
}

static const char *const ARTIFACT_TYPES[] = {"transcript", "recording", "call_record", NULL};

static int tm_artifact_type_ok(const json_t *v) {
    int i;
    if (!v || v->type != JSON_STRING) return 0;
    for (i = 0; ARTIFACT_TYPES[i]; i++) {
        if (!strcmp(v->str_val, ARTIFACT_TYPES[i])) return 1;
    }
    return 0;
}

/* _pick(payload, *aliases) for one field; the value is a fresh node. */
static json_t *tm_field_pick(const tm_field *f, const json_t *payload) {
    if (f->aliases[0]) {
        size_t n = 0;
        while (n < 4 && f->aliases[n]) n++;
        return w20_models_59cc22_pick(payload, f->aliases, n);
    } else {
        const char *keys[2];
        char *camel = w20_models_59cc22_camel(f->name);
        json_t *out;
        keys[0] = f->name;
        keys[1] = camel;
        out = w20_models_59cc22_pick(payload, keys, camel ? 2 : 1);
        free(camel);
        return out;
    }
}

static json_t *tm_from_dict(const tm_model *m, const json_t *payload, tm_err *err);
static json_t *tm_model_to_dict(const tm_model *m, const json_t *obj);

static int tm_convert_field(const tm_field *f, const json_t *payload,
                            json_t *out, tm_err *err) {
    json_t *raw = tm_field_pick(f, payload);
    json_t *conv = NULL;
    if (!raw) { tm_raise(err, "MemoryError", "out of memory"); return 0; }
    switch (f->conv) {
    case TMC_STR: {
        char *s = w20_models_59cc22_str(raw);
        if (!s) { json_free(raw); tm_raise(err, "MemoryError", "out of memory"); return 0; }
        conv = json_string(s);
        free(s);
        break;
    }
    case TMC_LIST:
        conv = tm_to_list(raw, err);
        break;
    case TMC_DICT:
        conv = tm_as_dict(raw, err);
        break;
    case TMC_NESTED_REF: {
        const tm_model *nested = tm_model_find(!strcmp(f->name, "summary_payload")
                                               ? "TeamsMeetingSummaryPayload" : "TeamsMeetingRef");
        if (!tm_truthy(raw) || !nested) {
            conv = json_null();
        } else {
            conv = tm_from_dict(nested, raw, err);
        }
        break;
    }
    case TMC_LIST_ARTIFACT: {
        const tm_model *art = tm_model_find("MeetingArtifact");
        if (!tm_truthy(raw) || !art) {
            conv = json_array();
        } else if (raw->type != JSON_ARRAY) {
            char msg[128];
            snprintf(msg, sizeof(msg), "'%s' object is not iterable", tm_type_name(raw));
            tm_raise(err, "TypeError", msg);
            conv = NULL;
        } else {
            json_t *arr = json_array();
            size_t i;
            conv = arr;
            for (i = 0; i < raw->c.count; i++) {
                json_t *item = tm_from_dict(art, raw->c.items[i], err);
                if (!item) { json_free(arr); conv = NULL; break; }
                json_append(arr, item);
            }
        }
        break;
    }
    case TMC_NONE:
    default:
        conv = json_copy(raw);
        break;
    }
    json_free(raw);
    if (!conv) return 0;
    json_set(out, f->name, conv);
    return 1;
}

static json_t *tm_instance(const tm_model *m, const json_t *payload, tm_err *err) {
    json_t *inst = json_object();
    size_t i;
    for (i = 0; i < m->nfields; i++) {
        if (!tm_convert_field(&m->fields[i], payload, inst, err)) {
            json_free(inst);
            return NULL;
        }
    }
    /* __post_init__: MeetingArtifact checks artifact_type before super(), then
     * _REQUIRED (field order), then _DATETIMES, then the subclass checks. */
    if (m->artifact_type_check) {
        json_t *at = json_obj_get(inst, "artifact_type");
        if (!tm_artifact_type_ok(at)) {
            tm_raise(err, "ValueError",
                     "MeetingArtifact.artifact_type must be transcript, recording, or call_record.");
            json_free(inst);
            return NULL;
        }
    }
    for (i = 0; i < m->nfields; i++) {
        const tm_field *f = &m->fields[i];
        json_t *v;
        if (!f->required) continue;
        v = json_obj_get(inst, f->name);
        if (!v || v->type != JSON_STRING || !v->str_val[0]) {
            char msg[256];
            snprintf(msg, sizeof(msg), "%s.%s is required.", m->name, f->name);
            tm_raise(err, "ValueError", msg);
            json_free(inst);
            return NULL;
        }
    }
    for (i = 0; i < m->nfields; i++) {
        const tm_field *f = &m->fields[i];
        json_t *v;
        char *wire;
        int ok = 0;
        if (!f->datetime) continue;
        v = json_obj_get(inst, f->name);
        wire = tm_datetime_wire(v, &ok);
        if (!ok) {
            char msg[512];
            char *raw = tm_str(v);
            snprintf(msg, sizeof(msg), "Invalid isoformat string: '%.400s'", raw ? raw : "");
            free(raw);
            tm_raise(err, "ValueError", msg);
            free(wire);
            json_free(inst);
            return NULL;
        }
        json_set(inst, f->name, wire ? json_string(wire) : json_null());
        free(wire);
    }
    if (!strcmp(m->name, "GraphSubscription")) {
        json_t *exp = json_obj_get(inst, "expiration_datetime");
        if (!exp || exp->type == JSON_NULL) {
            tm_raise(err, "ValueError", "GraphSubscription.expiration_datetime is required.");
            json_free(inst);
            return NULL;
        }
    }
    for (i = 0; i < m->nfields; i++) {
        const tm_field *f = &m->fields[i];
        json_t *v;
        long long iv;
        if (!f->coerce_int) continue;
        v = json_obj_get(inst, f->name);
        if (!v || v->type == JSON_NULL) continue;
        if (!tm_to_int(v, &iv, err)) { json_free(inst); return NULL; }
        json_set(inst, f->name, json_number((double)iv));
    }
    return inst;
}

/* from_dict: convert every field, then run __post_init__. */
static json_t *tm_from_dict(const tm_model *m, const json_t *payload, tm_err *err) {
    if (!payload || payload->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof(msg), "'%s' object has no attribute 'get'", tm_type_name(payload));
        tm_raise(err, "AttributeError", msg);
        return NULL;
    }
    return tm_instance(m, payload, err);
}

/* --- to_dict -------------------------------------------------------------- */

static json_t *tm_serialize_value(const tm_field *f, const json_t *v) {
    if (!v || v->type == JSON_NULL) return NULL;
    switch (f->conv) {
    case TMC_LIST:
    case TMC_LIST_ARTIFACT:
        if (v->type != JSON_ARRAY) return json_copy(v);
        if (v->c.count == 0) return NULL;
        if (f->conv == TMC_LIST_ARTIFACT) {
            const tm_model *art = tm_model_find("MeetingArtifact");
            json_t *arr = json_array();
            size_t i;
            for (i = 0; i < v->c.count; i++) {
                json_t *item = art ? tm_model_to_dict(art, v->c.items[i]) : json_copy(v->c.items[i]);
                if (!item) continue;
                json_append(arr, item);
            }
            return arr;
        }
        return json_copy(v);
    case TMC_DICT:
        if (v->type != JSON_OBJECT) return json_copy(v);
        if (v->c.count == 0) return NULL;
        return json_copy(v);
    case TMC_NESTED_REF: {
        const tm_model *nested;
        if (v->type != JSON_OBJECT || v->c.count == 0) return NULL;
        nested = tm_model_find(!strcmp(f->name, "summary_payload")
                               ? "TeamsMeetingSummaryPayload" : "TeamsMeetingRef");
        return nested ? tm_model_to_dict(nested, v) : json_copy(v);
    }
    case TMC_STR:
    case TMC_NONE:
    default:
        return json_copy(v);
    }
}

static json_t *tm_model_to_dict(const tm_model *m, const json_t *obj) {
    json_t *out = json_object();
    size_t i;
    for (i = 0; i < m->nfields; i++) {
        const tm_field *f = &m->fields[i];
        json_t *v = json_obj_get(obj, f->name);
        json_t *ser;
        if (!v) continue;
        ser = tm_serialize_value(f, v);
        if (!ser) continue;
        json_set(out, f->name, ser);
    }
    return out;
}

/* --- public entry points -------------------------------------------------- */

static json_t *tm_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value ? value : json_null());
    return out;
}

static json_t *tm_fail(const tm_err *err) {
    json_t *out = json_object();
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string(err->type[0] ? err->type : "Exception"));
    json_set(out, "message", json_string(err->message));
    return out;
}

json_t *w20_models_59cc22_from_dict(const char *model, const json_t *payload) {
    const tm_model *m = tm_model_find(model);
    tm_err err;
    json_t *inst;
    memset(&err, 0, sizeof(err));
    if (!m) {
        tm_raise(&err, "NameError", model ? model : "None");
        return tm_fail(&err);
    }
    /* TeamsMeetingSummaryPayload.from_dict reads payload["meeting_ref"] first. */
    if (!strcmp(m->name, "TeamsMeetingSummaryPayload")) {
        if (!payload || payload->type != JSON_OBJECT) {
            char msg[128];
            snprintf(msg, sizeof(msg), "'%s' object is not subscriptable", tm_type_name(payload));
            tm_raise(&err, "TypeError", msg);
            return tm_fail(&err);
        }
        if (!json_obj_get(payload, "meeting_ref")) {
            tm_raise(&err, "KeyError", "'meeting_ref'");
            return tm_fail(&err);
        }
    }
    inst = tm_from_dict(m, payload, &err);
    if (!inst) return tm_fail(&err);
    return tm_ok(inst);
}

json_t *w20_models_59cc22_to_dict(const char *model, const json_t *obj) {
    const tm_model *m = tm_model_find(model);
    if (!m) return json_null();
    if (!obj || obj->type != JSON_OBJECT) return json_object();
    return tm_model_to_dict(m, obj);
}

json_t *w20_models_59cc22_roundtrip(const char *model, const json_t *payload) {
    const tm_model *m = tm_model_find(model);
    tm_err err;
    json_t *inst;
    memset(&err, 0, sizeof(err));
    if (!m) {
        tm_raise(&err, "NameError", model ? model : "None");
        return tm_fail(&err);
    }
    if (!strcmp(m->name, "TeamsMeetingSummaryPayload") && payload &&
        payload->type == JSON_OBJECT && !json_obj_get(payload, "meeting_ref")) {
        tm_raise(&err, "KeyError", "'meeting_ref'");
        return tm_fail(&err);
    }
    inst = tm_from_dict(m, payload, &err);
    if (!inst) return tm_fail(&err);
    {
        json_t *wire = tm_model_to_dict(m, inst);
        json_free(inst);
        return tm_ok(wire);
    }
}
