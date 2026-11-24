/*
 * adapter_authorization.c -- real C11 port of the decision surface of upstream
 * Hermes plugins/platforms/discord/adapter_authorization.py.
 *
 * PoP: w21_adapteraut_272509_is_automated_message @ plugins/platforms/discord/adapter_authorization.py:is_automated_message
 * PoP: w21_adapteraut_272509_reauthorize @ plugins/platforms/discord/adapter_authorization.py:DiscordAuthorizationMixin.reauthorize_native_roles
 *
 * The native-authorization I/O itself (client.get_guild, guild.fetch_member,
 * the transport re-read of load_gateway_config) stays upstream: it needs a
 * live Discord connection and a ContextVar snapshot. Everything this module
 * decides -- author classification, the CSV role-set coercion, and each of the
 * early-return conditions of the reauthorization walk -- is here.
 */
#include "hermes_json.h"
#include "noc_w21_adapteraut_272509.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AA_MAX_ROLES 64

static int aa_truthy(const json_t *v) {
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

/* `isinstance(x, int)` for the decoded shapes: a JSON bool is an int subclass
 * in Python, and a number is an int only when it is integral. */
static int aa_is_py_int(const json_t *v) {
    double d;
    if (!v) return 0;
    if (v->type == JSON_BOOL) return 1;
    if (v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* `str(value)` for the scalar shapes a CSV item or an id can carry. */
static const char *aa_py_str(const json_t *v, char *buf, size_t cap) {
    if (!v) { snprintf(buf, cap, "None"); return buf; }
    switch (v->type) {
    case JSON_STRING: return v->str_val ? v->str_val : "";
    case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return buf;
    case JSON_NUMBER:
        if (aa_is_py_int(v)) snprintf(buf, cap, "%lld", (long long)v->num_val);
        else snprintf(buf, cap, "%.17g", v->num_val);
        return buf;
    case JSON_NULL:   snprintf(buf, cap, "None"); return buf;
    case JSON_ARRAY:  snprintf(buf, cap, "[]"); return buf;
    case JSON_OBJECT: snprintf(buf, cap, "{}"); return buf;
    }
    return buf;
}

static int aa_all_digits(const char *s) {
    if (!s || !*s) return 0;
    for (; *s; s++) {
        if (!isdigit((unsigned char)*s)) return 0;
    }
    return 1;
}

/* `int(x)`; 0 on success, -1 where CPython raises TypeError/ValueError.
 * Numbers (including bool, a float truncating toward zero) and decimal strings
 * with an optional sign convert; null, arrays, objects and prose do not. */
static int aa_py_int(const json_t *v, long long *out) {
    const char *s;
    char *end = NULL;
    long long n;
    if (!v) return -1;
    switch (v->type) {
    case JSON_BOOL:   *out = v->bool_val ? 1 : 0; return 0;
    case JSON_NUMBER:
        if (!isfinite(v->num_val)) return -1;
        *out = (long long)v->num_val;
        return 0;
    case JSON_STRING: break;
    default: return -1;
    }
    s = v->str_val ? v->str_val : "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
    if (!*s) return -1;
    if (*s == '+' || *s == '-') {
        if (!isdigit((unsigned char)s[1])) return -1;
    } else if (!isdigit((unsigned char)*s)) {
        return -1;
    }
    n = strtoll(s, &end, 10);
    if (!end) return -1;
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\f' || *end == '\v') end++;
    if (*end) return -1;
    *out = n;
    return 0;
}

/* --- is_automated_message ------------------------------------------------- */

int w21_adapteraut_272509_is_automated_message(const json_t *message) {
    json_t *author, *bot, *webhook_id;
    if (!message || message->type != JSON_OBJECT) return 0;
    author = json_obj_get(message, "author");
    bot = (author && author->type == JSON_OBJECT) ? json_obj_get(author, "bot") : NULL;
    if (aa_truthy(bot)) return 1;                      /* `author.bot or ...` */
    webhook_id = json_obj_get(message, "webhook_id");
    return aa_is_py_int(webhook_id);
}

/* --- reauthorize_native_roles -------------------------------------------- */

/* `{int(str(item).strip()) for item in items if str(item).strip().isdigit()}` */
static int aa_role_set(const json_t *items, long long *roles, int cap) {
    int n = 0;
    size_t i;
    if (!items || items->type != JSON_ARRAY) return 0;   /* empty set */
    for (i = 0; i < items->c.count && n < cap; i++) {
        char buf[64];
        const char *text = aa_py_str(items->c.items[i], buf, sizeof(buf));
        char stripped[96];
        size_t len, k = 0, j = 0;
        len = strlen(text);
        while (k < len && isspace((unsigned char)text[k])) k++;
        while (len > k && isspace((unsigned char)text[len - 1])) len--;
        for (; k < len && j + 1 < sizeof(stripped); k++) stripped[j++] = text[k];
        stripped[j] = '\0';
        if (!aa_all_digits(stripped)) continue;            /* `.isdigit()` */
        roles[n++] = strtoll(stripped, NULL, 10);
    }
    return n;                                             /* a set of ints */
}

static int aa_roles_equal(const long long *a, int na, const long long *b, int nb) {
    int i, j;
    if (na != nb) return 0;
    for (i = 0; i < na; i++) {
        int found = 0;
        for (j = 0; j < nb; j++) {
            if (a[i] == b[j]) { found = 1; break; }
        }
        if (!found) return 0;
    }
    return 1;
}

static int aa_roles_contain(const long long *roles, int n, long long id) {
    int i;
    for (i = 0; i < n; i++) {
        if (roles[i] == id) return 1;
    }
    return 0;
}

int w21_adapteraut_272509_reauthorize(const json_t *case_) {
    json_t *items, *items2, *chat_type, *guild_src, *client_v, *member, *temp;
    long long roles[AA_MAX_ROLES], roles2[AA_MAX_ROLES];
    int nroles, nroles2, client_present, dm, i;
    long long guild_id = 0, uid = 0, member_id = 0, member_guild = 0;

    if (!case_ || case_->type != JSON_OBJECT) return 0;
    items = json_obj_get(case_, "role_items");
    items2 = json_obj_get(case_, "role_items_2");
    client_v = json_obj_get(case_, "client");

    nroles = aa_role_set(items, roles, AA_MAX_ROLES);
    if (items2 && items2->type == JSON_ARRAY) {
        nroles2 = aa_role_set(items2, roles2, AA_MAX_ROLES);
    } else {
        nroles2 = nroles;
        for (i = 0; i < nroles; i++) roles2[i] = roles[i];
    }
    client_present = client_v ? aa_truthy(client_v) : 1;   /* self._client */

    chat_type = json_obj_get(case_, "chat_type");
    dm = (chat_type && chat_type->type == JSON_STRING && chat_type->str_val &&
          !strcmp(chat_type->str_val, "dm"));
    guild_src = dm ? json_obj_get(case_, "dm_guild_id") : json_obj_get(case_, "scope_id");

    /* `if not roles or guild_id is None or self._client is None: return False` */
    if (nroles == 0) return 0;
    if (!guild_src || guild_src->type == JSON_NULL) return 0;
    if (!client_present) return 0;

    /* try: guild = client.get_guild(int(guild_id)); uid = int(source.user_id) */
    if (aa_truthy(json_obj_get(case_, "get_guild_raises"))) return 0;
    if (aa_py_int(guild_src, &guild_id) != 0) return 0;
    if (aa_py_int(json_obj_get(case_, "user_id"), &uid) != 0) return 0;

    /* `if guild is None: return False` */
    temp = json_obj_get(case_, "guild_found");
    if (temp && !aa_truthy(temp)) return 0;

    /* await guild.fetch_member(uid) under a 10s wait_for */
    temp = json_obj_get(case_, "fetch_error");
    if (temp && temp->type == JSON_STRING && temp->str_val && temp->str_val[0]) return 0;

    /* `if self._client is not client or current_policy != (roles, guild_id)` */
    if (aa_truthy(json_obj_get(case_, "client_replaced"))) return 0;
    if (!aa_roles_equal(roles, nroles, roles2, nroles2)) return 0;

    member = json_obj_get(case_, "member");
    if (!member || member->type != JSON_OBJECT) return 0;
    if (aa_py_int(json_obj_get(member, "id"), &member_id) != 0) return 0;
    if (member_id != uid) return 0;
    if (aa_py_int(json_obj_get(member, "guild_id"), &member_guild) != 0) return 0;
    if (member_guild != guild_id) return 0;
    {
        json_t *role_ids = json_obj_get(member, "role_ids");
        size_t k;
        if (!role_ids || role_ids->type != JSON_ARRAY) return 0;  /* any(...) of nothing */
        for (k = 0; k < role_ids->c.count; k++) {
            long long rid;
            if (aa_py_int(role_ids->c.items[k], &rid) != 0) continue;
            if (aa_roles_contain(roles, nroles, rid)) return 1;
        }
    }
    return 0;
}
