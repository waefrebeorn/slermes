/*
 * owner_identity.c — C port of upstream Hermes gateway/owner_identity.py
 * (gate w8_owneridenti_8d7dcb).
 *
 * PoP: owneridenti_login_provider @ gateway/owner_identity.py:_login_provider
 * PoP: owneridenti_principal_subject @ gateway/owner_identity.py:principal_subject
 * PoP: owneridenti_same_owner @ gateway/owner_identity.py:same_owner
 * PoP: owneridenti_owners_agree @ gateway/owner_identity.py:owners_agree
 *
 * The auth-subject parser/label and the legacy owner rule are the exact
 * algorithms of gateway/session_identity.py and session_rules/legacy_local.py
 * that these functions delegate to; host facts are injected seams.
 */
#include "noc_w8_owneridenti_8d7dcb.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUTH_PREFIX "auth:v1:"

static const char *const BUNDLED_LOGINS[] = {
    "basic", "nous", "self-hosted", "session-token", "server-internal"
};
#define NBUNDLED ((int)(sizeof(BUNDLED_LOGINS) / sizeof(BUNDLED_LOGINS[0])))

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return 0;
}

static void trim_copy(const char *s, char *out, size_t cap) {
    if (!s) { out[0] = '\0'; return; }
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' ||
                     s[n-1] == '\r' || s[n-1] == '\f' || s[n-1] == '\v')) n--;
    if (n >= cap) n = cap - 1;
    memcpy(out, s, n);
    out[n] = '\0';
}

/* json.dumps(s, separators=(',', ':'), ensure_ascii=True) for one string. */
static void py_quote(const char *s, char *out, size_t cap) {
    size_t k = 0;
    out[k++] = '"';
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p && k + 14 < cap; ) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') { out[k++] = '\\'; out[k++] = (char)c; p++; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; p++; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; p++; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; p++; }
        else if (c == '\b') { out[k++] = '\\'; out[k++] = 'b'; p++; }
        else if (c == '\f') { out[k++] = '\\'; out[k++] = 'f'; p++; }
        else if (c < 0x20) {
            k += (size_t)snprintf(out + k, cap - k, "\\u%04x", (unsigned)c);
            p++;
        } else if (c < 0x80) { out[k++] = (char)c; p++; }
        else {
            unsigned int cp; int len;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; len = 2; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; len = 3; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; len = 4; }
            else { cp = 0xFFFDu; len = 1; }
            for (int i = 1; i < len && p[i]; i++) cp = (cp << 6) | (p[i] & 0x3Fu);
            p += len;
            if (cp <= 0xFFFF) k += (size_t)snprintf(out + k, cap - k, "\\u%04x", cp);
            else {
                cp -= 0x10000;
                k += (size_t)snprintf(out + k, cap - k, "\\u%04x\\u%04x",
                                      0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
            }
        }
    }
    out[k++] = '"';
    out[k] = '\0';
}

static int bundled(const char *p) {
    if (!p) return 0;
    for (int i = 0; i < NBUNDLED; i++)
        if (!strcmp(p, BUNDLED_LOGINS[i])) return 1;
    return 0;
}

static int json_array_has_string(const json_t *arr, const char *want) {
    if (!json_is_array(arr) || !want) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, want)) return 1;
    }
    return 0;
}

static int bundled_or_extra(const char *provider, const json_t *extra_logins) {
    return bundled(provider) || json_array_has_string(extra_logins, provider);
}

json_t *owneridenti_login_provider(const char *provider, const json_t *extra_logins) {
    return json_bool(bundled_or_extra(provider, extra_logins) ? true : false);
}

/* str(identity.get('user_id') or 'unbound') */
static void identity_subject(const json_t *identity, char *out, size_t cap) {
    json_t *v = json_obj_get(identity, "user_id");
    if (!truthy(v)) { snprintf(out, cap, "unbound"); return; }
    if (v->type == JSON_STRING) snprintf(out, cap, "%s", v->str_val);
    else if (v->type == JSON_BOOL) snprintf(out, cap, "%s", v->bool_val ? "True" : "False");
    else if (v->type == JSON_NUMBER) snprintf(out, cap, "%.17g", v->num_val);
    else snprintf(out, cap, "unbound");
}

/* exact algorithm of authenticated_subject() in gateway/session_identity.py */
static void authenticated_subject(const json_t *identity, char *out, size_t cap) {
    char subject[1024];
    identity_subject(identity, subject, sizeof(subject));
    json_t *pv = json_obj_get(identity, "provider");
    const char *provider = (pv && pv->type == JSON_STRING) ? pv->str_val : "";
    json_t *iv = json_obj_get(identity, "issuer");
    const char *issuer = (iv && iv->type == JSON_STRING) ? iv->str_val : "";
    if (!strcmp(provider, "local") && truthy(json_obj_get(identity, "profile_id"))
        && truthy(json_obj_get(identity, "instance_id"))
        && json_obj_get(identity, "capabilities")) {
        snprintf(out, cap, "%s", subject);
        return;
    }
    char a[4 * 1024], b[4 * 1024], c[4 * 1024];
    py_quote(provider, a, sizeof(a));
    py_quote(issuer, b, sizeof(b));
    py_quote(subject, c, sizeof(c));
    snprintf(out, cap, AUTH_PREFIX "[%s,%s,%s]", a, b, c);
}

/* parse_authenticated_subject(): auth:v1:[provider, issuer, user] -> 3 strings */
static int parse_auth(const char *subject, char p[256], char i[256], char u[256]) {
    p[0] = i[0] = u[0] = '\0';
    if (!subject || strncmp(subject, AUTH_PREFIX, strlen(AUTH_PREFIX))) return 0;
    json_t *arr = json_parse(subject + strlen(AUTH_PREFIX), NULL);
    if (!arr || arr->type != JSON_ARRAY || json_len(arr) != 3) { json_free(arr); return 0; }
    for (size_t k = 0; k < 3; k++) {
        json_t *e = json_get(arr, k);
        if (!e || e->type != JSON_STRING) { json_free(arr); return 0; }
    }
    snprintf(p, 256, "%s", json_get(arr, 0)->str_val);
    snprintf(i, 256, "%s", json_get(arr, 1)->str_val);
    snprintf(u, 256, "%s", json_get(arr, 2)->str_val);
    json_free(arr);
    return 1;
}

/* legacy_local.login_label(subject) */
static int login_label(const char *subject, char *out, size_t cap) {
    char p[256], i[256], u[256];
    if (!parse_auth(subject, p, i, u)) return 0;
    if (i[0] || strchr(p, ':')) return 0;
    char tp[256], tu[256];
    trim_copy(p, tp, sizeof(tp));
    trim_copy(u, tu, sizeof(tu));
    if (!tp[0] || !tu[0]) return 0;
    snprintf(out, cap, "%s:%s", tp, tu);
    if (!strncmp(out, "auth:", 5)) return 0;
    return 1;
}

json_t *owneridenti_principal_subject(const json_t *identity, const char *host_subject,
                                      const json_t *extra_logins) {
    char auth[8192];
    authenticated_subject(identity, auth, sizeof(auth));
    if (!truthy(json_obj_get(identity, "user_id"))) return json_string(auth);
    json_t *pv = json_obj_get(identity, "provider");
    char provider[256];
    trim_copy((pv && pv->type == JSON_STRING) ? pv->str_val : "", provider, sizeof(provider));
    if (!strcmp(provider, "local")) {
        char p[256], i[256], u[256];
        if (parse_auth(auth, p, i, u) && bundled_or_extra(p, extra_logins) && strcmp(p, "local"))
            return json_string(host_subject ? host_subject : "");
        return json_string(auth);
    }
    if (bundled_or_extra(provider, extra_logins))
        return json_string(host_subject ? host_subject : "");
    return json_string(auth);
}

/* legacy_local._owner_login(provider, user, rule) */
static int owner_login(const char *provider, const char *user, const json_t *rule) {
    char tu[256];
    trim_copy(user, tu, sizeof(tu));
    if (!strcmp(provider, "local")) {
        const char *host = json_get_str(rule, "host_subject", "");
        return !strcmp(tu, host);
    }
    return json_array_has_string(json_obj_get(rule, "login_providers"), provider);
}

/* legacy_local._login_subject + _login_stamp */
static int legacy_login_stamp(const char *value, const json_t *rule) {
    char p[256], i[256], u[256];
    if (parse_auth(value, p, i, u)) {
        char tu[256]; trim_copy(u, tu, sizeof(tu));
        return tu[0] && owner_login(p, u, rule);
    }
    const char *sep = strchr(value, ':');
    if (!sep || !sep[1]) return 0;
    size_t plen = (size_t)(sep - value);
    if (plen >= sizeof(p)) plen = sizeof(p) - 1;
    memcpy(p, value, plen); p[plen] = '\0';
    char tu[256]; trim_copy(sep + 1, tu, sizeof(tu));
    return tu[0] && strcmp(p, "nous") && owner_login(p, sep + 1, rule);
}

static int same_owner_core(const char *stored, const char *subject, const json_t *rule) {
    if ((!stored && !subject) || (stored && subject && !strcmp(stored, subject))) return 1;
    if (!stored || !stored[0] || !subject || !subject[0]) return 0;
    char label[1024];
    if (login_label(subject, label, sizeof(label)) && !strcmp(stored, label)) return 1;
    const char *host = json_get_str(rule, "host_subject", "");
    if (host[0] && !strcmp(subject, host) && legacy_login_stamp(stored, rule)) return 1;
    return 0;
}

json_t *owneridenti_same_owner(const char *stored, const char *subject, const json_t *rule) {
    return json_bool(same_owner_core(stored, subject, rule) ? true : false);
}
json_t *owneridenti_owners_agree(const char *first, const char *second, const json_t *rule) {
    return json_bool((same_owner_core(first, second, rule) || same_owner_core(second, first, rule))
                     ? true : false);
}
