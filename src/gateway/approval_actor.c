/*
 * approval_actor.c — C port of upstream Hermes gateway/approval_actor.py.
 *
 * PoP: approvalact_may_answer @ gateway/approval_actor.py:may_answer
 * PoP: approvalact_refusal @ gateway/approval_actor.py:_refusal
 * PoP: approvalact_commands @ gateway/approval_actor.py:approval_commands
 *
 * gateway.slash_access is a late import in the Python source; the oracle
 * registers a scripted SlashAccessPolicy/policy_for_chat_type/_coerce_id_list,
 * mirrored here from the fixture's `extra` / `admin_ids` / `commands` fields.
 */
#include "noc_w8_approvalact_58fb2a.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *xstrdup(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}
static char *py_strip(const char *s) {
    if (!s) return xstrdup("");
    const char *a = s, *b = s + strlen(s);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    char *r = (char *)malloc((size_t)(b - a) + 1);
    memcpy(r, a, (size_t)(b - a)); r[b - a] = '\0';
    return r;
}
static const char *opt_str(json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static int arr_has(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = opt_str(json_get(arr, i));
        if (v && !strcmp(v, s)) return 1;
    }
    return 0;
}
static int policy_enabled(json_t *extra) {
    if (!extra || extra->type != JSON_OBJECT) return 0;
    json_t *v = json_obj_get(extra, "enabled");
    return v && v->type == JSON_BOOL && v->bool_val;
}
static json_t *policy_admins(json_t *extra, json_t *admin_ids) {
    json_t *out = json_array();
    json_t *src = NULL;
    if (extra && extra->type == JSON_OBJECT) src = json_obj_get(extra, "admins");
    else src = admin_ids;
    if (src && src->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(src); i++) {
            json_t *v = json_get(src, i);
            const char *s = opt_str(v);
            if (s) json_append(out, json_string(s));
        }
    return out;
}
static json_t *policy_commands(json_t *extra) {
    json_t *out = json_array();
    if (extra && extra->type == JSON_OBJECT) {
        json_t *src = json_obj_get(extra, "commands");
        if (src && src->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(src); i++) {
                const char *s = opt_str(json_get(src, i));
                if (s) json_append(out, json_string(s));
            }
    }
    return out;
}
/* _coerce_id_list(authority.get("approvers")) */
static json_t *coerce_approvers(json_t *v) {
    json_t *out = json_array();
    if (!v || v->type == JSON_NULL) return out;
    if (v->type == JSON_STRING) { json_append(out, json_copy(v)); return out; }
    if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(v); i++) {
            json_t *e = json_get(v, i);
            const char *s = opt_str(e);
            json_append(out, json_string(s ? s : ""));
        }
        return out;
    }
    if (v->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(v); i++)
            json_append(out, json_string(json_object_get_key_at(v, i)));
    }
    return out;
}
static int can_run(const char *actor, const char *cmd, json_t *admins, json_t *allowed) {
    if (actor && actor[0] && arr_has(admins, actor)) return 1;
    return arr_has(allowed, cmd);
}

char *approvalact_refusal(json_t *authority, const char *actor_id,
                          json_t *admin_ids, json_t *extra,
                          json_t *commands, int admitted) {
    char *actor = py_strip(actor_id);
    json_t *admins = policy_admins(extra, admin_ids);
    json_t *allowed = policy_commands(extra);
    int enabled = policy_enabled(extra);
    json_t *approvers = coerce_approvers(authority ? json_obj_get(authority, "approvers") : NULL);
    char *result = NULL;
    if (arr_has(approvers, "*")) {
        if (commands && commands->type == JSON_ARRAY && json_len(commands) > 0 &&
            enabled) {
            int any = 0;
            for (size_t i = 0; i < json_len(commands); i++) {
                const char *c = opt_str(json_get(commands, i));
                if (c && can_run(actor[0] ? actor : NULL, c, admins, allowed)) { any = 1; break; }
            }
            if (!any) {
                const char *first = opt_str(json_get(commands, 0));
                result = xstrdup(first ? first : ""); /* admin-only marker (may_answer only checks NULL) */
                goto done;
            }
        }
        if (admitted) { result = NULL; goto done; }
    }
    const char *requester = opt_str(authority ? json_obj_get(authority, "requester_id") : NULL);
    if (!requester) requester = "";
    if (actor[0] && (!strcmp(actor, requester) || arr_has(approvers, actor) ||
                     arr_has(admins, actor))) {
        result = NULL; goto done;
    }
    const char *name = opt_str(authority ? json_obj_get(authority, "requester_name") : NULL);
    if (name && name[0]) result = xstrdup(name);
    else if (requester[0]) result = xstrdup(requester);
    else result = xstrdup("NO_REQUESTER"); /* sentinel marker */
done:
    free(actor); json_free(admins); json_free(allowed); json_free(approvers);
    return result;
}

int approvalact_may_answer(json_t *authority, const char *actor_id,
                           json_t *admin_ids, json_t *extra,
                           json_t *commands, int admitted) {
    char *r = approvalact_refusal(authority, actor_id, admin_ids, extra, commands, admitted);
    int allowed = (r == NULL);
    free(r);
    return allowed;
}

json_t *approvalact_commands(const char *choice) {
    json_t *out = json_array();
    if (!choice) {
        json_append(out, json_string("approve"));
        json_append(out, json_string("deny"));
    } else if (!strcmp(choice, "deny")) {
        json_append(out, json_string("deny"));
    } else {
        json_append(out, json_string("approve"));
    }
    return out;
}
