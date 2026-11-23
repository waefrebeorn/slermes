/*
 * hosted_rooms.c — C port of upstream Hermes session_rules/hosted_rooms.py.
 *
 * PoP: w14_hostedroom_765f11_identifier @ session_rules/hosted_rooms.py:identifier
 * PoP: w14_hostedroom_765f11_bounded_int @ session_rules/hosted_rooms.py:bounded_int
 * PoP: w14_hostedroom_765f11_text @ session_rules/hosted_rooms.py:text
 * PoP: w14_hostedroom_765f11_validate_context @ session_rules/hosted_rooms.py:validate_context
 * PoP: w14_hostedroom_765f11_context_from_payload @ session_rules/hosted_rooms.py:context_from_payload
 * PoP: w14_hostedroom_765f11_retry_fields @ session_rules/hosted_rooms.py:retry_fields
 * PoP: w14_hostedroom_765f11_retained_fields @ session_rules/hosted_rooms.py:retained_fields
 *
 * Pure leaf validators / canonicalizers. Python exceptions are surfaced as
 * {"error": "<message>"} so the differential oracle can compare them.
 */
#include "noc_w14_hostedroom_765f11.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PURPOSE_CHARS 500
#define MAX_NOTES_BYTES 4096

static const char *hr_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static json_t *hr_err(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg ? msg : ""));
    return o;
}
static char *hr_strip(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *p = (char *)malloc(b - a + 1);
    if (!p) return NULL;
    memcpy(p, s + a, b - a);
    p[b - a] = '\0';
    return p;
}
static size_t hr_utf8len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if ((*p & 0xC0) == 0x80) continue;
        if (*p < 0x80) n++;
        else if (*p < 0xE0) n += 2;
        else if (*p < 0xF0) n += 3;
        else n += 4;
    }
    return n;
}
static long long hr_int(const json_t *v, long long def) {
    if (v && v->type == JSON_NUMBER) return (long long)v->num_val;
    return def;
}
/* Python " ".join(value.split()) */
static char *hr_collapse_ws(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 2);
    if (!out) return NULL;
    size_t o = 0;
    size_t i = 0;
    while (i < n) {
        while (i < n && isspace((unsigned char)s[i])) i++;
        if (i >= n) break;
        if (o) out[o++] = ' ';
        while (i < n && !isspace((unsigned char)s[i])) out[o++] = s[i++];
    }
    out[o] = '\0';
    return out;
}
/* pattern: 1=IDENTIFIER_RE, 2=event kind, 0=none */
static int hr_shape_ok(const char *s, int mode) {
    if (!*s) return 0;
    unsigned char c0 = (unsigned char)s[0];
    if (mode == 1) {
        if (!isalnum(c0)) return 0;
        for (const char *p = s + 1; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (!isalnum(c) && c != '.' && c != '_' && c != ':' && c != '-') return 0;
        }
        return 1;
    }
    if (mode == 2) {
        if (!(c0 >= 'a' && c0 <= 'z')) return 0;
        for (const char *p = s + 1; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (!((c >= 'a' && c <= 'z') || isdigit(c) || c == '_' || c == '.' || c == '-')) return 0;
        }
        return 1;
    }
    return 1;
}

json_t *w14_hostedroom_765f11_identifier(const json_t *value, const json_t *label,
        const json_t *max_chars, const json_t *pattern_mode, const json_t *invalid) {
    const char *lab = hr_jstr(label);
    if (!lab) lab = "value";
    char buf[512];
    if (!value || value->type != JSON_STRING || !value->str_val) {
        snprintf(buf, sizeof(buf), "%s must be a string", lab);
        return hr_err(buf);
    }
    char *v = hr_strip(value->str_val);
    if (!v) return hr_err("out of memory");
    long long mx = hr_int(max_chars, 128);
    const char *pm = hr_jstr(pattern_mode);
    int mode = 1;
    if (pm && !strcmp(pm, "none")) mode = 0;
    else if (pm && !strcmp(pm, "event_kind")) mode = 2;
    const char *inv = hr_jstr(invalid);
    size_t len = hr_utf8len(v);
    if (!*v || len > (size_t)mx || !hr_shape_ok(v, mode)) {
        if (inv) snprintf(buf, sizeof(buf), "%s", inv);
        else snprintf(buf, sizeof(buf), "invalid %s", lab);
        free(v);
        return hr_err(buf);
    }
    json_t *out = json_string(v);
    free(v);
    return out;
}

json_t *w14_hostedroom_765f11_bounded_int(const json_t *value, const json_t *message,
        const json_t *low, const json_t *high) {
    const char *msg = hr_jstr(message);
    if (!msg) msg = "invalid integer";
    int bad = 0;
    long long v = 0;
    if (!value || value->type == JSON_BOOL || value->type != JSON_NUMBER ||
        value->num_val != (double)(long long)value->num_val) {
        bad = 1;
    } else {
        v = (long long)value->num_val;
        if (v < hr_int(low, 0)) bad = 1;
        if (!bad && high && high->type == JSON_NUMBER && v > (long long)high->num_val) bad = 1;
    }
    if (bad) return hr_err(msg);
    return json_number((double)v);
}

json_t *w14_hostedroom_765f11_text(const json_t *value, const json_t *label,
        const json_t *max_bytes, const json_t *strip) {
    const char *lab = hr_jstr(label);
    if (!lab) lab = "value";
    char buf[512];
    if (!value || value->type != JSON_STRING || !value->str_val) {
        snprintf(buf, sizeof(buf), "%s must be a string", lab);
        return hr_err(buf);
    }
    char *stripped = hr_strip(value->str_val);
    if (!stripped) return hr_err("out of memory");
    if (!*stripped) {
        snprintf(buf, sizeof(buf), "%s must not be empty", lab);
        free(stripped);
        return hr_err(buf);
    }
    int do_strip = 1;
    if (strip && strip->type == JSON_BOOL) do_strip = strip->bool_val ? 1 : 0;
    const char *chosen = do_strip ? (const char *)stripped : (const char *)value->str_val;
    long long mb = hr_int(max_bytes, 0);
    if (hr_utf8len(chosen) > (size_t)mb) {
        snprintf(buf, sizeof(buf), "%s is too large", lab);
        free(stripped);
        return hr_err(buf);
    }
    json_t *out = json_string(chosen);
    free(stripped);
    return out;
}

json_t *w14_hostedroom_765f11_validate_context(const json_t *purpose, const json_t *notes,
        const json_t *routing, const json_t *lead_member_id) {
    if (!hr_jstr(purpose) || !hr_jstr(notes) || !hr_jstr(routing) || !hr_jstr(lead_member_id))
        return hr_err("purpose, notes, routing and lead_member_id must be strings");
    char *p = hr_collapse_ws(purpose->str_val);
    char *n = hr_strip(notes->str_val);
    if (!p || !n) { free(p); free(n); return hr_err("out of memory"); }
    if (strlen(p) > MAX_PURPOSE_CHARS) { free(p); free(n); return hr_err("purpose must be at most 500 characters"); }
    if (hr_utf8len(n) > MAX_NOTES_BYTES) { free(p); free(n); return hr_err("notes must be at most 4096 bytes"); }
    const char *r = routing->str_val;
    if (strcmp(r,"everyone") && strcmp(r,"lead") && strcmp(r,"auto")) {
        free(p); free(n); return hr_err("routing must be 'everyone', 'lead' or 'auto'");
    }
    char *lmid = hr_strip(lead_member_id->str_val);
    if (!strcmp(r,"lead") && (!lmid || !*lmid)) { free(p); free(n); free(lmid); return hr_err("lead routing needs a lead_member_id"); }
    json_t *out = json_object();
    json_set(out, "purpose", json_string(p));
    json_set(out, "notes", json_string(n));
    json_set(out, "routing", json_string(r));
    json_set(out, "lead_member_id", json_string(!strcmp(r,"lead") && lmid ? lmid : ""));
    free(p); free(n); free(lmid);
    return out;
}

static const char *hr_get_or(const json_t *obj, const char *key, const char *def) {
    const char *s = hr_jstr(json_is_object(obj) ? json_obj_get(obj, key) : NULL);
    return s ? s : def;
}
json_t *w14_hostedroom_765f11_context_from_payload(const json_t *payload) {
    json_t *out = json_object();
    if (!json_is_object(payload)) payload = NULL;
    json_set(out, "purpose", json_string(hr_get_or(payload,"purpose","")));
    json_set(out, "notes", json_string(hr_get_or(payload,"notes","")));
    json_set(out, "routing", json_string(hr_get_or(payload,"routing","everyone")));
    json_set(out, "lead_member_id", json_string(hr_get_or(payload,"lead_member_id","")));
    return out;
}

json_t *w14_hostedroom_765f11_retry_fields(const json_t *context, const json_t *original) {
    json_t *out = json_object();
    if (!json_is_object(context)) return out;
    json_t *empty = w14_hostedroom_765f11_context_from_payload(NULL);
    for (size_t i = 0; i < json_object_size(context); i++) {
        const char *k = json_object_get_key_at(context, i);
        json_t *v = json_object_get_at(context, i);
        if (!k) continue;
        int present = json_is_object(original) && json_obj_get(original, k) != NULL;
        const char *dv = hr_jstr(json_obj_get(empty, k));
        if (!dv) dv = "";
        const char *vv = hr_jstr(v);
        if (present || (vv && strcmp(vv, dv))) json_set(out, k, json_copy(v));
    }
    json_free(empty);
    return out;
}

json_t *w14_hostedroom_765f11_retained_fields(const json_t *table) {
    json_t *out = json_array();
    const char *t = hr_jstr(table);
    if (!t) t = "";
    json_append(out, json_string("room_id"));
    json_append(out, json_string("revision"));
    json_append(out, json_string("digest"));
    json_append(out, json_string("record_json"));
    if (!strcmp(t, "hosted_room_work_records_invalid"))
        json_append(out, json_string("source_table"));
    else {
        json_append(out, json_string("producer_gateway_id"));
        json_append(out, json_string("producer_epoch"));
    }
    if (!strcmp(t, "hosted_room_work_records_pending") || !strcmp(t, "hosted_room_work_records_invalid")) {
        json_append(out, json_string("target_install_id"));
        json_append(out, json_string("route_generation"));
        json_append(out, json_string("status"));
    }
    return out;
}
