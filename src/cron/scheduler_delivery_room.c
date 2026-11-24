/*
 * scheduler_delivery_room.c — C port of upstream Hermes cron/scheduler_delivery_room.py.
 *
 * PoP: w16_schedulerd_dc3aeb_parse_room_deliver_token @ cron/scheduler_delivery_room.py:parse_room_deliver_token
 * PoP: w16_schedulerd_dc3aeb_resolve_room_target @ cron/scheduler_delivery_room.py:resolve_room_target
 * PoP: w16_schedulerd_dc3aeb_profile_hint @ cron/scheduler_delivery_room.py:_profile_hint
 *
 * Only the pure token/target/hint logic is ported; the store/DB and uuid paths in
 * deliver_to_room stay outside this gate.
 */
#include "noc_w16_schedulerd_dc3aeb.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}
static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%.17g", v->num_val);
        return dupstr(b);
    }
    return dupstr("");
}
static const char *WS = " \t\n\r\f\v";
static char *py_strip(const char *s) {
    if (!s) return dupstr("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && strchr(WS, s[a])) a++;
    while (b > a && strchr(WS, s[b - 1])) b--;
    char *p = (char *)malloc(b - a + 1);
    if (p) { memcpy(p, s + a, b - a); p[b - a] = '\0'; }
    return p;
}
static int lower_ascii(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
/* raw.lower().startswith("room:") */
static int ci_prefix_room(const char *s) {
    static const char pre[] = "room:";
    for (int i = 0; i < 5; i++)
        if (!s[i] || lower_ascii(s[i]) != pre[i]) return 0;
    return 1;
}
static int alnum_ascii(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}
static int room_char_ok(char c) {
    return alnum_ascii(c) || c == '.' || c == '_' || c == ':' || c == '-';
}
static int profile_char_ok(char c) {
    return alnum_ascii(c) || c == '_' || c == '-';
}
/* _ROOM_TOKEN_RE.fullmatch: ^[A-Za-z0-9][A-Za-z0-9._:-]*(@[A-Za-z0-9_-]+)?$ */
static int room_rest_valid(const char *rest) {
    if (!rest || !rest[0]) return 0;
    if (!alnum_ascii(rest[0])) return 0;
    const char *at = strchr(rest, '@');
    size_t room_len = at ? (size_t)(at - rest) : strlen(rest);
    if (room_len == 0) return 0;
    for (size_t i = 0; i < room_len; i++)
        if (!room_char_ok(rest[i])) return 0;
    if (at) {
        const char *prof = at + 1;
        if (!*prof) return 0;
        for (const char *p = prof; *p; p++)
            if (!profile_char_ok(*p)) return 0;
    }
    return 1;
}

json_t *w16_schedulerd_dc3aeb_parse_room_deliver_token(const json_t *part) {
    char *raw;
    if (!py_truthy(part) || !part || part->type != JSON_STRING)
        raw = dupstr("");
    else
        raw = py_strip(part->str_val ? part->str_val : "");
    if (!raw) return json_null();
    if (!ci_prefix_room(raw)) { free(raw); return json_null(); }
    char *rest = py_strip(raw + 5);
    free(raw);
    json_t *out;
    if (rest && room_rest_valid(rest)) out = json_string(rest);
    else out = json_string("");
    free(rest);
    return out;
}

json_t *w16_schedulerd_dc3aeb_resolve_room_target(const json_t *job, const json_t *address) {
    (void)job; /* only read for a warning log in Python */
    if (!py_truthy(address)) return json_null();
    json_t *o = json_object();
    json_set(o, "platform", json_string("room"));
    json_set(o, "chat_id", json_copy(address));
    json_set(o, "thread_id", json_null());
    return o;
}

json_t *w16_schedulerd_dc3aeb_profile_hint(const json_t *room_id, const json_t *profile) {
    char *lead = dupstr("");
    if (py_truthy(profile)) {
        char *ps = py_str(profile);
        size_t n = strlen(ps) + 48;
        char *buf = (char *)malloc(n);
        if (buf) snprintf(buf, n, "@%s does not host it. ", ps);
        free(ps);
        if (buf) { free(lead); lead = buf; }
    }
    char *rs = py_str(room_id);
    size_t total = strlen(lead) + strlen(rs) + 220;
    char *out = (char *)malloc(total);
    if (out)
        snprintf(out, total,
                 "%sIf a profile's gateway hosts this room, deliver to room:%s@<that profile> (Room settings shows the exact target).",
                 lead, rs);
    free(lead); free(rs);
    json_t *j = json_string(out ? out : "");
    free(out);
    return j;
}
