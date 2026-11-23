/*
 * legacy_local.c — C port of upstream Hermes session_rules/legacy_local.py.
 *
 * PoP: w14_legacyloca_18b83d_parse_authenticated_subject @ session_rules/legacy_local.py:parse_authenticated_subject
 * PoP: w14_legacyloca_18b83d_login_label @ session_rules/legacy_local.py:login_label
 * PoP: w14_legacyloca_18b83d_same_owner @ session_rules/legacy_local.py:same_owner
 * PoP: w14_legacyloca_18b83d_owners_agree @ session_rules/legacy_local.py:owners_agree
 * PoP: w14_legacyloca_18b83d_binding_names @ session_rules/legacy_local.py:binding_names
 * PoP: w14_legacyloca_18b83d_bindings_agree @ session_rules/legacy_local.py:bindings_agree
 * PoP: w14_legacyloca_18b83d_client_label @ session_rules/legacy_local.py:client_label
 * PoP: w14_legacyloca_18b83d_legacy_history_row @ session_rules/legacy_local.py:legacy_history_row
 * PoP: w14_legacyloca_18b83d_legacy_creation_source @ session_rules/legacy_local.py:legacy_creation_source
 * PoP: w14_legacyloca_18b83d_legacy_account_label @ session_rules/legacy_local.py:legacy_account_label
 * PoP: w14_legacyloca_18b83d_login_owned_by @ session_rules/legacy_local.py:login_owned_by
 * PoP: w14_legacyloca_18b83d_validate_legacy_row @ session_rules/legacy_local.py:validate_legacy_row
 *
 * Owner/binding/label rules for unrouted legacy local history. The OwnerRule and
 * vocabulary sets are mirrored as constants; AuthorityRefusal surfaces as
 * {"error": "not_found"}.
 */
#include "noc_w14_legacyloca_18b83d.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUTH_PREFIX "auth:v1:"

static const char *const LOCAL_SOURCES[] = {"cli","tui","gui","desktop","oneshot","tool","acp",NULL};
static const char *const NOT_CLIENT[] = {
    "cron","subagent","kanban","bot_room","a2a","webhook","api_server","import","recovered",
    "curator","batch","homeassistant","webui","local","unknown",NULL};

static const char *ll_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static char *ll_strip(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *p = (char *)malloc(b - a + 1);
    if (!p) return NULL;
    memcpy(p, s + a, b - a);
    p[b - a] = '\0';
    return p;
}
static int ll_in(const char *const *set, const char *s) {
    if (!s) return 0;
    for (int i = 0; set[i]; i++) if (!strcmp(set[i], s)) return 1;
    return 0;
}
static int ll_eq(const json_t *a, const json_t *b) {
    if (a == b) return 1;
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING:
            return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (json_len(a) != json_len(b)) return 0;
            for (size_t i = 0; i < json_len(a); i++) if (!ll_eq(json_get(a,i), json_get(b,i))) return 0;
            return 1;
        case JSON_OBJECT:
            if (json_object_size(a) != json_object_size(b)) return 0;
            for (size_t i = 0; i < json_object_size(a); i++) {
                const char *k = json_object_get_key_at(a, i);
                const json_t *vb = json_obj_get(b, k);
                if (!vb || !ll_eq(json_object_get_at(a,i), vb)) return 0;
            }
            return 1;
    }
    return 0;
}
static int ll_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0;
        case JSON_STRING: return v->str_val && *v->str_val;
        case JSON_ARRAY: return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default: return 0;
    }
}

/* Rule seam: host_subject (str), login_providers (array), messaging_labels (array). */
static const char *ll_rule_host(const json_t *r) {
    return ll_jstr(json_is_object(r) ? json_obj_get(r, "host_subject") : NULL);
}
static int ll_rule_has_provider(const json_t *r, const char *p) {
    if (!r || !p) return 0;
    const json_t *arr = json_obj_get(r, "login_providers");
    if (!json_is_array(arr)) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = ll_jstr(json_get(arr, i));
        if (v && !strcmp(v, p)) return 1;
    }
    return 0;
}
static int ll_rule_has_messaging(const json_t *r, const char *p) {
    if (!r || !p) return 0;
    const json_t *arr = json_obj_get(r, "messaging_labels");
    if (!json_is_array(arr)) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = ll_jstr(json_get(arr, i));
        if (v && !strcmp(v, p)) return 1;
    }
    return 0;
}

/* Returns an owned array of 3 strings, or NULL. Caller frees. */
static json_t *ll_parse_subject(const char *s) {
    if (!s || strncmp(s, AUTH_PREFIX, 8)) return NULL;
    json_t *parts = json_parse(s + 8, NULL);
    if (!json_is_array(parts) || json_len(parts) != 3) { json_free(parts); return NULL; }
    for (size_t i = 0; i < 3; i++) {
        if (json_get(parts,i)->type != JSON_STRING) { json_free(parts); return NULL; }
    }
    return parts;
}
static char *ll_login_label(const char *subject) {
    if (!subject) return NULL;
    json_t *parts = ll_parse_subject(subject);
    if (!parts) return NULL;
    const char *provider = json_get(parts,0)->str_val;
    const char *issuer = json_get(parts,1)->str_val;
    const char *user = json_get(parts,2)->str_val;
    char *p = ll_strip(provider), *u = ll_strip(user);
    char *out = NULL;
    if (!*issuer && !strchr(p, ':') && *p && *u) {
        size_t need = strlen(p) + strlen(u) + 2;
        char *lab = (char *)malloc(need);
        if (lab) {
            snprintf(lab, need, "%s:%s", p, u);
            if (!strncmp(lab, "auth:", 5)) { free(lab); }
            else out = lab;
        }
    }
    free(p); free(u);
    json_free(parts);
    return out;
}
static int ll_owner_login(const char *provider, const char *user, const json_t *rule) {
    if (!provider) return 0;
    if (!strcmp(provider, "local")) {
        char *u = ll_strip(user ? user : "");
        const char *host = ll_rule_host(rule);
        int ok = host && u && !strcmp(u, host);
        free(u);
        return ok;
    }
    return ll_rule_has_provider(rule, provider);
}
static int ll_login_stamp(const char *value, const json_t *rule) {
    if (!value) return 0;
    const char *sep = strchr(value, ':');
    if (!sep) return 0;
    size_t plen = (size_t)(sep - value);
    if (plen >= 256) return 0;
    char provider[256];
    memcpy(provider, value, plen); provider[plen] = '\0';
    const char *user = sep + 1;
    char *u = ll_strip(user);
    int ok = *u && strcmp(provider, "nous") && ll_owner_login(provider, u, rule);
    free(u);
    return ok;
}
static int ll_login_subject(const char *value, const json_t *rule) {
    if (!value) return 0;
    json_t *parts = ll_parse_subject(value);
    if (!parts) return 0;
    char *u = ll_strip(json_get(parts,2)->str_val);
    int ok = *u && ll_owner_login(json_get(parts,0)->str_val, u, rule);
    free(u);
    json_free(parts);
    return ok;
}
static json_t *ll_b(const int v) { return json_bool(v ? 1 : 0); }

json_t *w14_legacyloca_18b83d_parse_authenticated_subject(const struct json_t *subject) {
    return ll_parse_subject(ll_jstr(subject));
}
json_t *w14_legacyloca_18b83d_login_label(const struct json_t *subject) {
    char *lab = ll_login_label(ll_jstr(subject));
    if (!lab) return NULL;
    json_t *out = json_string(lab);
    free(lab);
    return out;
}
json_t *w14_legacyloca_18b83d_same_owner(const struct json_t *stored, const struct json_t *subject, const struct json_t *rule) {
    if (ll_eq(stored, subject)) return ll_b(1);
    const char *st = ll_jstr(stored);
    if (!st || !*st || !ll_truthy(subject)) return ll_b(0);
    char *lab = ll_login_label(ll_jstr(subject));
    int res = 0;
    if (lab && !strcmp(st, lab)) res = 1;
    else if (rule && !json_is_null(rule) && ll_jstr(subject) && !strcmp(ll_jstr(subject), ll_rule_host(rule) ? ll_rule_host(rule) : "")
             && (ll_login_subject(st, rule) || ll_login_stamp(st, rule))) res = 1;
    free(lab);
    return ll_b(res);
}
json_t *w14_legacyloca_18b83d_owners_agree(const struct json_t *first, const struct json_t *second, const struct json_t *rule) {
    json_t *a = w14_legacyloca_18b83d_same_owner(first, second, rule);
    json_t *b = w14_legacyloca_18b83d_same_owner(second, first, rule);
    int res = (a->bool_val || b->bool_val);
    json_free(a); json_free(b);
    return ll_b(res);
}
/* _binding_owner -> owned json_t* of the subject, or NULL. Returns NULL both for
 * unreadable and for None owner; callers only need "is not None" + the value. */
static json_t *ll_binding_owner(const char *saved) {
    if (!saved) return NULL;
    json_t *v = json_parse(saved, NULL);
    if (!json_is_array(v) || json_len(v) != 2 || json_get(v,0)->type != JSON_STRING) { json_free(v); return NULL; }
    json_t *owner = json_copy(json_get(v,1));
    json_free(v);
    return owner;
}
json_t *w14_legacyloca_18b83d_binding_names(const struct json_t *saved, const struct json_t *subject, const struct json_t *rule) {
    json_t *owner = ll_binding_owner(ll_jstr(saved));
    if (!owner) return ll_b(0);
    json_t *r = w14_legacyloca_18b83d_same_owner(owner, subject, rule);
    int res = r->bool_val;
    json_free(r); json_free(owner);
    return ll_b(res);
}
json_t *w14_legacyloca_18b83d_bindings_agree(const struct json_t *first, const struct json_t *second, const struct json_t *rule) {
    if (ll_eq(first, second) || first->type == JSON_NULL || second->type == JSON_NULL)
        return ll_b(ll_eq(first, second));
    json_t *fo = ll_binding_owner(ll_jstr(first));
    json_t *so = ll_binding_owner(ll_jstr(second));
    int res = 0;
    if (fo && so) {
        json_t *a = w14_legacyloca_18b83d_same_owner(fo, so, rule);
        json_t *b = w14_legacyloca_18b83d_same_owner(so, fo, rule);
        res = (a->bool_val || b->bool_val);
        json_free(a); json_free(b);
    }
    json_free(fo); json_free(so);
    return ll_b(res);
}
static int ll_not_client(const char *label, const json_t *rule) {
    return ll_in(LOCAL_SOURCES, label) || ll_in(NOT_CLIENT, label) || ll_rule_has_messaging(rule, label);
}
json_t *w14_legacyloca_18b83d_client_label(const struct json_t *source, const struct json_t *rule) {
    const char *s = ll_jstr(source);
    if (!rule || json_is_null(rule) || !s || !*s) return ll_b(0);
    char *t = ll_strip(s);
    int ok = *t && !ll_not_client(t, rule);
    free(t);
    return ll_b(ok);
}
static int ll_client_history_row(const json_t *row, const json_t *rule) {
    if (!json_is_object(row)) return 0;
    json_t *src = json_obj_get(row, "source");
    json_t *cl = w14_legacyloca_18b83d_client_label(src, rule);
    int ok = cl->bool_val;
    json_free(cl);
    if (!ok) return 0;
    const char *keys[] = {"session_key","chat_id","origin_json",NULL};
    for (int i = 0; keys[i]; i++) if (ll_truthy(json_obj_get(row, keys[i]))) return 0;
    return 1;
}
json_t *w14_legacyloca_18b83d_legacy_history_row(const struct json_t *row, const struct json_t *rule) {
    const char *src = json_is_object(row) ? ll_jstr(json_obj_get(row,"source")) : NULL;
    if (ll_in(LOCAL_SOURCES, src)) return ll_b(1);
    return ll_b(ll_client_history_row(row, rule));
}
json_t *w14_legacyloca_18b83d_legacy_creation_source(const struct json_t *row, const struct json_t *rule) {
    if (ll_client_history_row(row, rule)) return json_string("gui");
    const char *src = json_is_object(row) ? ll_jstr(json_obj_get(row,"source")) : NULL;
    if (src && !strcmp(src, "desktop")) return json_string("gui");
    return json_string(src ? src : "");
}
json_t *w14_legacyloca_18b83d_legacy_account_label(const struct json_t *row) {
    if (!json_is_object(row)) return ll_b(0);
    const char *src = ll_jstr(json_obj_get(row,"source"));
    const char *uid = ll_jstr(json_obj_get(row,"user_id"));
    return ll_b(src && !strcmp(src,"desktop") && uid && !strncmp(uid,"nous:",5));
}
json_t *w14_legacyloca_18b83d_login_owned_by(const struct json_t *row, const struct json_t *subject, const struct json_t *rule) {
    const char *uid = json_is_object(row) ? ll_jstr(json_obj_get(row,"user_id")) : NULL;
    if (!uid || !ll_truthy(subject)) return ll_b(0);
    json_t *u = json_string(uid);
    json_t *r = w14_legacyloca_18b83d_same_owner(u, subject, rule);
    int res = r->bool_val;
    json_free(r); json_free(u);
    return ll_b(res);
}
json_t *w14_legacyloca_18b83d_validate_legacy_row(const struct json_t *own_profile, const struct json_t *row,
        const struct json_t *subject, const struct json_t *rule) {
    int bad = 0;
    json_t *hist = w14_legacyloca_18b83d_legacy_history_row(row, rule);
    if (!hist->bool_val) bad = 1;
    json_free(hist);
    const char *keys[] = {"session_key","chat_id","origin_json",NULL};
    if (json_is_object(row)) for (int i = 0; keys[i] && !bad; i++) if (ll_truthy(json_obj_get(row,keys[i]))) bad = 1;
    const json_t *uid = json_is_object(row) ? json_obj_get(row,"user_id") : NULL;
    if (!bad && uid && ll_truthy(uid)) {
        json_t *al = w14_legacyloca_18b83d_legacy_account_label(row);
        json_t *lo = w14_legacyloca_18b83d_login_owned_by(row, subject, rule);
        if (!(al->bool_val || lo->bool_val)) bad = 1;
        json_free(al); json_free(lo);
    }
    const char *pn = json_is_object(row) ? ll_jstr(json_obj_get(row,"profile_name")) : NULL;
    const char *own = ll_jstr(own_profile);
    const char *def = own ? own : "default";
    if (pn && *pn && strcmp(pn, def)) bad = 1;
    if (bad) {
        json_t *o = json_object();
        json_set(o, "error", json_string("not_found"));
        return o;
    }
    return json_null();
}
