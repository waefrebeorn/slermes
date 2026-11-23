/*
 * route_entries.c — C home for upstream Hermes session_rules/route_entries.py.
 *
 * PoP: w15_routeentri_26892c_sanitize_prompt_pin @ session_rules/route_entries.py:sanitize_prompt_pin
 * PoP: w15_routeentri_26892c_sanitize_model_override @ session_rules/route_entries.py:sanitize_model_override
 * PoP: w15_routeentri_26892c_is_path_unsafe @ session_rules/route_entries.py:_is_path_unsafe
 * PoP: w15_routeentri_26892c_session_key_namespace @ session_rules/route_entries.py:_session_key_namespace
 * PoP: w15_routeentri_26892c_build_session_key @ session_rules/route_entries.py:build_session_key
 *
 * Every exported symbol is w15_routeentri_26892c_ prefixed. Strings are copied
 * out of input nodes before any tree is freed; no input node is mutated.
 */
#include "noc_w15_routeentri_26892c.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static int nonnull_str(const json_t *v) {
    const char *s = jstr(v);
    return s && *s;
}

/* Python truthiness of a decoded JSON value. */
static int py_bool(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

json_t *w15_routeentri_26892c_sanitize_prompt_pin(const json_t *pin) {
    if (!pin || pin->type != JSON_OBJECT) return NULL;
    const json_t *ver = json_obj_get(pin, "version");
    int version_ok = (ver && ver->type == JSON_NUMBER && ver->num_val == 1.0) ||
                     (ver && ver->type == JSON_BOOL && ver->bool_val);
    if (!version_ok) return NULL;
    const json_t *ck = json_obj_get(pin, "context_key");
    const json_t *cp = json_obj_get(pin, "context_prompt");
    const json_t *rp = json_obj_get(pin, "redact_pii");
    if (!nonnull_str(ck) || !jstr(cp) || !rp || rp->type != JSON_BOOL) return NULL;
    const json_t *ch = json_obj_get(pin, "channel_prompt");
    const json_t *pc = json_obj_get(pin, "parent_chat_id");
    if (ch && ch->type != JSON_NULL && !jstr(ch)) return NULL;
    if (pc && pc->type != JSON_NULL && !jstr(pc)) return NULL;

    json_t *out = json_object();
    json_set(out, "version", json_number(1));
    json_set(out, "context_key", json_string(jstr(ck)));
    json_set(out, "context_prompt", json_string(jstr(cp)));
    json_set(out, "redact_pii", json_bool(rp->bool_val));
    json_set(out, "channel_prompt", jstr(ch) ? json_string(jstr(ch)) : json_null());
    json_set(out, "parent_chat_id", jstr(pc) ? json_string(jstr(pc)) : json_null());
    return out;
}

json_t *w15_routeentri_26892c_sanitize_model_override(const json_t *override) {
    if (!override || override->type != JSON_OBJECT) return NULL;
    json_t *out = json_object();
    for (size_t i = 0; i < json_object_size(override); ++i) {
        const char *k = json_object_get_key_at(override, i);
        const json_t *v = json_object_get_at(override, i);
        if (!k) continue;
        if (strcmp(k, "model") && strcmp(k, "provider") && strcmp(k, "base_url")) continue;
        if (!v || v->type == JSON_NULL) continue;
        if (v->type == JSON_STRING && v->str_val[0] == '\0') continue;
        const char *s = jstr(v);
        if (s) { json_set(out, k, json_string(s)); continue; }
        if (v->type == JSON_BOOL) { json_set(out, k, json_string(v->bool_val ? "True" : "False")); continue; }
        if (v->type == JSON_NUMBER) {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d) snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else snprintf(buf, sizeof(buf), "%g", d);
            json_set(out, k, json_string(buf));
        }
    }
    if (json_object_size(out) == 0) { json_free(out); return NULL; }  /* cleaned or None */
    return out;
}

int w15_routeentri_26892c_is_path_unsafe(const json_t *value, int strict) {
    if (!py_bool(value)) return 0;
    const char *s = jstr(value);
    if (!s) {
        /* str(non-string): numbers/bools only; fixtures keep these out. */
        return 0;
    }
    if (strstr(s, "..")) return 1;
    if (strict && (strchr(s, '/') || strchr(s, '\\'))) return 1;
    if (!strict && (s[0] == '/' || s[0] == '\\')) return 1;
    return strlen(s) >= 2 && isalpha((unsigned char)s[0]) && s[1] == ':';
}

char *w15_routeentri_26892c_session_key_namespace(const char *profile) {
    if (!profile || !*profile || !strcmp(profile, "default")) return dupstr("agent:main");
    if (!strcmp(profile, "main")) return dupstr("agent:main~");
    size_t n = strlen(profile) + strlen("agent:") + 1;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "agent:%s", profile);
    return out;
}

/* Growable string buffer for the ':' joined key. */
typedef struct { char *s; size_t len, cap; } sbuf;
static void sb_add(sbuf *b, const char *part) {
    size_t n = strlen(part);
    if (!b->s) {
        b->cap = 64;
        b->s = (char *)malloc(b->cap);
        if (!b->s) return;
        b->s[0] = '\0';
    }
    if (b->len + n + 2 > b->cap) {
        size_t cap = b->cap * 2;
        while (cap < b->len + n + 2) cap *= 2;
        char *p = (char *)realloc(b->s, cap);
        if (!p) return;
        b->s = p; b->cap = cap;
    }
    if (b->len) b->s[b->len++] = ':';
    memcpy(b->s + b->len, part, n);
    b->len += n;
    b->s[b->len] = '\0';
}

char *w15_routeentri_26892c_build_session_key(const json_t *source,
                                              int group_sessions_per_user,
                                              int thread_sessions_per_user,
                                              const char *profile) {
    if (!source || source->type != JSON_OBJECT) return dupstr("");
    const char *detached = jstr(json_obj_get(source, "detached_route"));
    if (detached && *detached) return dupstr(detached);

    const char *platform = jstr(json_obj_get(source, "platform"));
    if (!platform) platform = "";
    const char *chat_id = jstr(json_obj_get(source, "chat_id"));
    if (!chat_id) chat_id = "";
    const char *chat_type = jstr(json_obj_get(source, "chat_type"));
    if (!chat_type) chat_type = "dm";
    int is_dm = !strcmp(chat_type, "dm");

    const char *raw_thread = jstr(json_obj_get(source, "thread_id"));
    const char *prosp = jstr(json_obj_get(source, "prospective_thread_id"));
    const char *thread = NULL;
    if (raw_thread && *raw_thread) thread = raw_thread;
    else if (!is_dm && prosp && *prosp) thread = prosp;
    const char *slot = (thread && !(raw_thread && *raw_thread)) ? "thread" : chat_type;

    int shared = py_bool(json_obj_get(source, "shared_session"));
    int isolate;
    if (is_dm) isolate = !*chat_id;
    else isolate = group_sessions_per_user && !(thread && !thread_sessions_per_user) && !shared;

    const char *alt = jstr(json_obj_get(source, "user_id_alt"));
    const char *uid = jstr(json_obj_get(source, "user_id"));
    const char *participant = NULL;
    if (isolate || !is_dm) {
        if (alt && *alt) participant = alt;
        else if (uid && *uid) participant = uid;
    }

    char *ns = w15_routeentri_26892c_session_key_namespace(profile);
    sbuf b = {0};
    sb_add(&b, ns ? ns : "agent:main");
    free(ns);
    sb_add(&b, platform);
    sb_add(&b, slot);
    const char *scope = jstr(json_obj_get(source, "scope_id"));
    if (!strcmp(platform, "slack") && scope && *scope) sb_add(&b, scope);
    if (*chat_id) sb_add(&b, chat_id);
    if (is_dm) {
        if (isolate && participant && *participant) sb_add(&b, participant);
        if (thread && *thread) sb_add(&b, thread);
    } else {
        if (thread && *thread) sb_add(&b, thread);
        if (isolate && participant && *participant) sb_add(&b, participant);
    }
    if (!b.s) return dupstr("");
    return b.s;
}
