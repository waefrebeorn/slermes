/*
 * adapter_component_auth.c -- real C11 port of the pure authorization decisions of
 * upstream Hermes plugins/platforms/discord/adapter_component_auth.py.
 *
 * PoP: w22_adaptercom_02f6a6_chat_context @ plugins/platforms/discord/adapter_component_auth.py:component_chat_context
 * PoP: w22_adaptercom_02f6a6_check_auth @ plugins/platforms/discord/adapter_component_auth.py:_component_check_auth
 * PoP: w22_adaptercom_02f6a6_opener_requester @ plugins/platforms/discord/adapter_component_auth.py:opener_refusal
 *
 * Seams, all injected by the caller (see the header): the profile secret-scope
 * env read, the pairing store, the per-call live authorization callback, the
 * `/update` marker read and the i18n catalog. Everything else -- the allow-all
 * short circuit, the `*`/uid/role grants, the fail-closed role branch, and the
 * approval-actor refusal rule (`_authority_refusal` -> `_refusal` with no
 * command floor) -- is the live rule.
 */
#include "hermes_json.h"
#include "noc_w22_adaptercom_02f6a6.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AC_MAX_IDS 256
#define AC_ARENA   32768

/* Python str() for the scalar shapes a wire id / chat id carries. */
static char *ac_py_str(const json_t *v) {
    char buf[64];
    const char *src;
    size_t n;
    char *out;
    if (!v || v->type == JSON_NULL) return NULL;
    switch (v->type) {
    case JSON_BOOL:
        src = v->bool_val ? "True" : "False";
        break;
    case JSON_NUMBER:
        if (isfinite(v->num_val) && floor(v->num_val) == v->num_val &&
            v->num_val >= -9.007199254740992e15 && v->num_val <= 9.007199254740992e15) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else {
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        }
        src = buf;
        break;
    case JSON_STRING:
        src = v->str_val ? v->str_val : "";
        break;
    default:
        return NULL; /* containers have no wire spelling here */
    }
    n = strlen(src);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, src, n + 1);
    return out;
}

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int ac_truthy(const json_t *v) {
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

static int ac_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* An id set of strings, arena-backed: no allocation to leak on any path. */
typedef struct {
    char  arena[AC_ARENA];
    size_t used;
    const char *v[AC_MAX_IDS];
    int n;
} ac_strset;

static void ac_set_add(ac_strset *s, const char *val) {
    size_t n;
    int i;
    if (!val) return;
    while (*val && ac_is_space(*val)) val++;
    n = strlen(val);
    while (n > 0 && ac_is_space(val[n - 1])) n--;
    if (n == 0) return;                                  /* `if str(uid).strip()` */
    for (i = 0; i < s->n; i++) {
        if (strlen(s->v[i]) == n && !strncmp(s->v[i], val, n)) return;
    }
    if (s->n >= AC_MAX_IDS || s->used + n + 1 > sizeof(s->arena)) return;
    memcpy(s->arena + s->used, val, n);
    s->arena[s->used + n] = '\0';
    s->v[s->n++] = s->arena + s->used;
    s->used += n + 1;
}

static int ac_set_has(const ac_strset *s, const char *val) {
    int i;
    if (!val) return 0;
    for (i = 0; i < s->n; i++) {
        if (!strcmp(s->v[i], val)) return 1;
    }
    return 0;
}

/* `_coerce_list`: None -> empty; list -> each str(it).strip(); comma string ->
 * its non-empty fields; any other scalar -> itself. Always strings. */
static void ac_coerce_list(const json_t *raw, ac_strset *out) {
    size_t i;
    if (!raw || raw->type == JSON_NULL) return;
    if (raw->type == JSON_ARRAY) {
        for (i = 0; i < raw->c.count; i++) {
            char *t = ac_py_str(raw->c.items[i]);
            ac_set_add(out, t);
            free(t);
        }
        return;
    }
    if (raw->type == JSON_STRING) {
        const char *p = raw->str_val ? raw->str_val : "";
        char field[512];
        size_t k = 0;
        for (;;) {
            char c = *p;
            if (c == ',' || c == '\0') {
                field[k] = '\0';
                ac_set_add(out, field);
                k = 0;
                if (c == '\0') break;
                p++;
                continue;
            }
            if (k + 1 < sizeof(field)) field[k++] = c;
            p++;
        }
        return;
    }
    {
        char *t = ac_py_str(raw);
        ac_set_add(out, t);
        free(t);
    }
}

/* The gate env read seam: the fixture carries the profile's scoped value. */
static const char *ac_env(const json_t *spec, const char *name) {
    const json_t *env = spec ? json_obj_get(spec, "env") : NULL;
    const json_t *v;
    if (!env || env->type != JSON_OBJECT) return "";
    v = json_obj_get(env, name);
    if (!v || v->type != JSON_STRING || !v->str_val) return "";
    return v->str_val;
}

/* `strip().lower() in {"true", "1", "yes"}` */
static int ac_env_flag_true(const char *raw) {
    char buf[32];
    size_t n, i;
    if (!raw) return 0;
    n = strlen(raw);
    while (n > 0 && ac_is_space(raw[n - 1])) n--;
    while (n > 0 && ac_is_space(*raw)) { raw++; n--; }
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    for (i = 0; i < n; i++) {
        char c = raw[i];
        buf[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    buf[n] = '\0';
    return !strcmp(buf, "true") || !strcmp(buf, "1") || !strcmp(buf, "yes");
}

/* A type-tagged key, so `{1, 2} & {"1"}` (Python !=) stays distinct. */
static void ac_tagged(const json_t *v, char *out, size_t cap) {
    char *t;
    if (!v || v->type == JSON_NULL) { snprintf(out, cap, "z"); return; }
    if (v->type == JSON_NUMBER) { snprintf(out, cap, "i:%.17g", v->num_val); return; }
    if (v->type == JSON_BOOL) { snprintf(out, cap, "b:%d", v->bool_val ? 1 : 0); return; }
    t = ac_py_str(v);
    snprintf(out, cap, "s:%s", t ? t : "");
    free(t);
}

/* `set(allowed_role_ids or set())`: the raw values, type-tagged, because Python
 * `{"12"} & {12}` is empty -- a str id is not the int id it spells. A bare
 * string is iterated as its characters, exactly as `set("ab")` is. */
static void ac_tagged_set(const json_t *raw, ac_strset *out) {
    size_t i;
    if (!raw || raw->type == JSON_NULL) return;
    if (raw->type == JSON_ARRAY) {
        for (i = 0; i < raw->c.count; i++) {
            char key[160];
            ac_tagged(raw->c.items[i], key, sizeof(key));
            ac_set_add(out, key);
        }
        return;
    }
    if (raw->type == JSON_STRING) {
        const char *p = raw->str_val ? raw->str_val : "";
        for (; *p; p++) {
            char one[2];
            char key[16];
            json_t tmp;
            one[0] = *p;
            one[1] = '\0';
            memset(&tmp, 0, sizeof(tmp));
            tmp.type = JSON_STRING;
            tmp.str_val = one;
            ac_tagged(&tmp, key, sizeof(key));
            ac_set_add(out, key);
        }
        return;
    }
    {
        char key[160];
        ac_tagged(raw, key, sizeof(key));
        ac_set_add(out, key);
    }
}

/* --- component_chat_context ---------------------------------------------- */

json_t *w22_adaptercom_02f6a6_chat_context(const json_t *interaction) {
    const json_t *in = (interaction && interaction->type == JSON_OBJECT) ? interaction : NULL;
    const json_t *cid = in ? json_obj_get(in, "channel_id") : NULL;
    const json_t *guild = in ? json_obj_get(in, "guild") : NULL;
    const json_t *kind = in ? json_obj_get(in, "channel_kind") : NULL;
    const json_t *tcls = in ? json_obj_get(in, "thread_cls") : NULL;
    char *chat_id = (cid && cid->type != JSON_NULL) ? ac_py_str(cid) : NULL;
    const char *thread_name = (tcls && tcls->type == JSON_STRING && tcls->str_val)
                                  ? tcls->str_val : "Thread";
    json_t *out = json_array();

    if (!guild || guild->type == JSON_NULL) {
        json_append(out, json_string("dm"));
        json_append(out, chat_id ? json_string(chat_id) : json_null());
        json_append(out, json_null());
        free(chat_id);
        return out;
    }
    if (kind && kind->type == JSON_STRING && kind->str_val && *thread_name &&
        !strcmp(kind->str_val, thread_name)) {
        json_append(out, json_string("thread"));
        json_append(out, chat_id ? json_string(chat_id) : json_null());
        json_append(out, chat_id ? json_string(chat_id) : json_null());
        free(chat_id);
        return out;
    }
    json_append(out, json_string("group"));
    json_append(out, chat_id ? json_string(chat_id) : json_null());
    json_append(out, json_null());
    free(chat_id);
    return out;
}

/* --- _component_check_auth ----------------------------------------------- */

int w22_adaptercom_02f6a6_check_auth(const json_t *spec) {
    const json_t *inter, *user, *idv, *roles, *live;
    ac_strset users = {{0}, 0, {0}, 0};
    ac_strset roles_allow = {{0}, 0, {0}, 0};
    ac_strset user_role_ids = {{0}, 0, {0}, 0};
    char *uid = NULL;
    int rc = 0;

    if (!spec || spec->type != JSON_OBJECT) return 0;
    inter = json_obj_get(spec, "interaction");
    user = (inter && inter->type == JSON_OBJECT) ? json_obj_get(inter, "user") : NULL;
    if (!user || user->type != JSON_OBJECT) return 0;    /* `user is None` */
    idv = json_obj_get(user, "id");
    if (!idv || idv->type == JSON_NULL) return 0;        /* `user.id is None` */

    if (ac_env_flag_true(ac_env(spec, "DISCORD_ALLOW_ALL_USERS"))) return 1;
    if (ac_env_flag_true(ac_env(spec, "GATEWAY_ALLOW_ALL_USERS"))) return 1;

    ac_coerce_list(json_obj_get(spec, "allowed_user_ids"), &users);
    ac_tagged_set(json_obj_get(spec, "allowed_role_ids"), &roles_allow);
    {
        /* `global_allowed`: the comma-split, stripped GATEWAY_ALLOWED_USERS. */
        json_t *tmp = json_string(ac_env(spec, "GATEWAY_ALLOWED_USERS"));
        ac_coerce_list(tmp, &users);
        json_free(tmp);
    }

    uid = ac_py_str(idv);
    if (uid && !uid[0]) { free(uid); uid = NULL; }

    live = json_obj_get(spec, "live_auth");              /* absent / "none" -> None */
    {
        int live_false = (live && live->type == JSON_BOOL && !live->bool_val) ? 1 : 0;
        int live_none = (!live || live->type == JSON_NULL ||
                         (live->type == JSON_STRING && live->str_val &&
                          !strcmp(live->str_val, "none")));
        if (users.n > 0) {
            int hit = ac_set_has(&users, "*") || (uid && ac_set_has(&users, uid));
            if (hit && (!live_false || live_none)) { rc = 1; goto done; }
        }
    }

    if (roles_allow.n > 0) {
        roles = json_obj_get(user, "roles");
        if (!roles || roles->type == JSON_NULL) { rc = 0; goto done; }  /* fail closed */
        if (roles->type == JSON_NUMBER || roles->type == JSON_BOOL) {
            rc = 0;                                                     /* TypeError */
            goto done;
        }
        if (roles->type == JSON_ARRAY) {
            size_t i;
            for (i = 0; i < roles->c.count; i++) {
                char key[160];
                const json_t *el = roles->c.items[i];
                if (el && el->type == JSON_OBJECT) ac_tagged(json_obj_get(el, "id"), key, sizeof(key));
                else ac_tagged(NULL, key, sizeof(key));                 /* getattr(r,'id',None) */
                ac_set_add(&user_role_ids, key);
            }
        } else if (roles->type == JSON_STRING) {
            if (roles->str_val && roles->str_val[0]) {
                char key[160];
                ac_tagged(NULL, key, sizeof(key));                      /* chars carry no .id */
                ac_set_add(&user_role_ids, key);
            }
        }
        {
            int i;
            for (i = 0; i < user_role_ids.n; i++) {
                if (ac_set_has(&roles_allow, user_role_ids.v[i])) { rc = 1; goto done; }
            }
        }
    }

    /* Pairing store: paired users click without an allowlist. */
    if (uid && ac_truthy(json_obj_get(spec, "paired"))) rc = 1;

done:
    free(uid);
    return rc;
}

/* --- opener_refusal ------------------------------------------------------ */

#define AC_NO_REQUESTER "\\0no-requester"   /* Python "\0no-requester", literal text */

/* policy_for_chat_type(extra, chat_type) -> admin_user_ids. With no command
 * floor (opener_refusal passes no commands) only the admins can matter. */
static void ac_policy_admins(const json_t *extra, const json_t *chat_type, ac_strset *out) {
    const json_t *ex = (extra && extra->type == JSON_OBJECT) ? extra : NULL;
    ac_strset dm = {{0}, 0, {0}, 0};
    ac_strset group = {{0}, 0, {0}, 0};
    int i;

    ac_coerce_list(ex ? json_obj_get(ex, "allow_admin_from") : NULL, &dm);
    ac_coerce_list(ex ? json_obj_get(ex, "group_allow_admin_from") : NULL, &group);

    if (chat_type && chat_type->type != JSON_NULL) {
        char norm[64];
        char *t = ac_py_str(chat_type);
        const char *p = t ? t : "";
        size_t n = strlen(p), k, j;
        while (n > 0 && ac_is_space(p[n - 1])) n--;
        while (n > 0 && ac_is_space(*p)) { p++; n--; }
        if (n >= sizeof(norm)) n = sizeof(norm) - 1;
        for (j = 0; j < n; j++) {
            char c = p[j];
            norm[j] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
        }
        norm[n] = '\0';
        k = n;
        free(t);
        if (k > 0) {
            int is_dm = !strcmp(norm, "dm") || !strcmp(norm, "direct") || !strcmp(norm, "private");
            if (is_dm) { for (i = 0; i < dm.n; i++) ac_set_add(out, dm.v[i]); }
            else { for (i = 0; i < group.n; i++) ac_set_add(out, group.v[i]); }
            return;
        }
    }
    /* Blank/None chat_type: both scopes gated -> only what both grant. */
    if (dm.n > 0 && group.n > 0) {
        for (i = 0; i < dm.n; i++) {
            if (ac_set_has(&group, dm.v[i])) ac_set_add(out, dm.v[i]);
        }
        return;
    }
    {
        const ac_strset *pick = dm.n > 0 ? &dm : &group;
        for (i = 0; i < pick->n; i++) ac_set_add(out, pick->v[i]);
    }
}

/* `_refusal(authority, actor, (), extra, admitted=True)` -> requester key or NULL. */
static char *ac_authority_refusal(const json_t *authority, const char *actor_in, const json_t *extra) {
    const json_t *auth = (authority && authority->type == JSON_OBJECT) ? authority : NULL;
    ac_strset approvers = {{0}, 0, {0}, 0};
    ac_strset admins = {{0}, 0, {0}, 0};
    char *actor = NULL, *requester = NULL, *name = NULL;
    const char *p = actor_in ? actor_in : "";
    size_t n = strlen(p);
    char *out = NULL;

    if (!auth || auth->c.count == 0) return NULL;        /* empty authority: anyone admitted */

    while (n > 0 && ac_is_space(p[n - 1])) n--;
    while (n > 0 && ac_is_space(*p)) { p++; n--; }
    actor = (char *)malloc(n + 1);
    if (!actor) return NULL;
    memcpy(actor, p, n);
    actor[n] = '\0';

    ac_coerce_list(json_obj_get(auth, "approvers"), &approvers);
    if (ac_set_has(&approvers, "*")) {
        /* `commands` is empty for a picker / `/update` answer so the slash floor
         * cannot fire, and `admitted` is the default True. */
        free(actor);
        return NULL;
    }
    ac_policy_admins(extra, json_obj_get(auth, "chat_type"), &admins);

    requester = ac_py_str(json_obj_get(auth, "requester_id"));   /* str(x or "") */
    if (!requester) {
        json_t *empty = json_string("");
        requester = ac_py_str(empty);
        json_free(empty);
    }
    if (!requester) { free(actor); return NULL; }
    if (actor[0]) {
        if (!strcmp(actor, requester) || ac_set_has(&approvers, actor) || ac_set_has(&admins, actor)) {
            free(actor);
            free(requester);
            return NULL;
        }
    }

    name = ac_py_str(json_obj_get(auth, "requester_name"));
    if (name && name[0]) {
        out = name;
        name = NULL;                                     /* ownership moves to `out` */
    } else if (requester[0]) {
        out = requester;
        requester = NULL;
    } else {
        size_t k = strlen(AC_NO_REQUESTER);
        out = (char *)malloc(k + 1);
        if (out) memcpy(out, AC_NO_REQUESTER, k + 1);
    }
    free(actor);
    free(name);
    free(requester);
    return out;
}

json_t *w22_adaptercom_02f6a6_opener_requester(const json_t *view, const json_t *interaction) {
    const json_t *v = (view && view->type == JSON_OBJECT) ? view : NULL;
    const json_t *inter = (interaction && interaction->type == JSON_OBJECT) ? interaction : NULL;
    const json_t *user = inter ? json_obj_get(inter, "user") : NULL;
    const json_t *idv = (user && user->type == JSON_OBJECT) ? json_obj_get(user, "id") : NULL;
    const json_t *extra = v ? json_obj_get(v, "answer_extra") : NULL;
    const json_t *authority = NULL;
    char *actor, *requester;
    json_t *out;

    actor = (idv && idv->type != JSON_NULL) ? ac_py_str(idv) : NULL;

    if (v && ac_truthy(json_obj_get(v, "has_on_selected"))) {
        authority = json_obj_get(v, "prompt_authority");
    } else if (v && ac_truthy(json_obj_get(v, "is_update_prompt"))) {
        authority = json_obj_get(v, "update_authority");   /* marker-file seam */
    } else {
        free(actor);
        return json_null();
    }

    requester = ac_authority_refusal(authority, actor ? actor : "", extra);
    free(actor);
    if (!requester) return json_null();
    out = json_string(requester);
    free(requester);
    return out;
}
