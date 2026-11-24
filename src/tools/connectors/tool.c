/*
 * tool.c - C home for tools/connectors/tool.py.
 * Faithful port of the manage_connections entry point and the target normalization /
 * action validation it runs before dispatch. The connector client, MCP backend and
 * managed-action executor are injected seams (live network/broker I/O stays Python).
 * PoP: w19_tool_6f4666_normalize_targets @ tools/connectors/targets.py:normalize_targets
 * PoP: w19_tool_6f4666_validate_action @ tools/connectors/targets.py:validate_action
 * PoP: w19_tool_6f4666_manage_connections @ tools/connectors/tool.py:manage_connections
 */
#include "noc_w19_tool_6f4666.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const MCP_ACTIONS[] = {"install", "enable", "authorize"};
static const char *const ALL_ACTIONS[] = {"display", "status", "connect", "reconnect", "rename",
                                          "install", "enable", "authorize"};
#define N_ACTIONS(a) (sizeof(a) / sizeof((a)[0]))

static const char *const ALIAS_FORMAT =
    "1-32 lowercase letters, digits or '-', starting with a letter or digit";

static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

static int py_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *py_strip_dup(const char *s) {
    const char *a;
    const char *b;
    size_t n;
    char *out;
    if (!s) return dupstr("");
    a = s;
    while (*a && py_ws((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && py_ws((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* str.lower() for the ASCII range (connector slugs are ASCII). */
static char *py_lower_dup(const char *s) {
    char *p = dupstr(s ? s : "");
    size_t i;
    if (!p) return NULL;
    for (i = 0; p[i]; i++)
        if (p[i] >= 'A' && p[i] <= 'Z') p[i] = (char)(p[i] - 'A' + 'a');
    return p;
}

/* Python repr() of a str: single quotes unless the text has a ' and no ". */
static char *py_repr_dup(const char *s) {
    const char *src = s ? s : "";
    char quote = '\'';
    size_t i, need = 2, pos = 0;
    char *out;
    if (strchr(src, '\'') && !strchr(src, '"')) quote = '"';
    for (i = 0; src[i]; i++) {
        unsigned char c = (unsigned char)src[i];
        need += (c == '\\' || c == (unsigned char)quote || c == '\n' || c == '\t' || c == '\r')
                ? 2 : 1;
    }
    out = (char *)malloc(need + 1);
    if (!out) return dupstr("");
    out[pos++] = quote;
    for (i = 0; src[i]; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '\\' || c == (unsigned char)quote) { out[pos++] = '\\'; out[pos++] = (char)c; }
        else if (c == '\n') { out[pos++] = '\\'; out[pos++] = 'n'; }
        else if (c == '\t') { out[pos++] = '\\'; out[pos++] = 't'; }
        else if (c == '\r') { out[pos++] = '\\'; out[pos++] = 'r'; }
        else out[pos++] = (char)c;
    }
    out[pos++] = quote;
    out[pos] = '\0';
    return out;
}

/* str.join over a const list */
static char *join_list(const char *const *items, size_t n) {
    size_t i, len = 1;
    char *out;
    for (i = 0; i < n; i++) len += strlen(items[i]) + 2;
    out = (char *)malloc(len);
    if (!out) return dupstr("");
    out[0] = '\0';
    for (i = 0; i < n; i++) {
        if (i) strcat(out, ", ");
        strcat(out, items[i]);
    }
    return out;
}

static int in_list(const char *const *items, size_t n, const char *v) {
    size_t i;
    if (!v) return 0;
    for (i = 0; i < n; i++)
        if (!strcmp(items[i], v)) return 1;
    return 0;
}

/* re.fullmatch(r"^[a-z0-9][a-z0-9-]{0,31}$", value) */
static int alias_ok(const char *v) {
    size_t i, n;
    if (!v) return 0;
    n = strlen(v);
    if (n < 1 || n > 32) return 0;
    if (!((v[0] >= 'a' && v[0] <= 'z') || (v[0] >= '0' && v[0] <= '9'))) return 0;
    for (i = 1; i < n; i++) {
        char c = v[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return 0;
    }
    return 1;
}

static const char *jstr_or_null(json_t *obj, const char *key) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* Python truthiness of a JSON value. */
static int jtruthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* _bad_alias(key, value) */
static char *bad_alias(const char *key, const char *value) {
    char *rep, *out;
    if (!value || alias_ok(value)) return NULL;
    rep = py_repr_dup(value);
    out = (char *)malloc(strlen(key) + strlen(rep) + strlen(ALIAS_FORMAT) + 64);
    if (!out) { free(rep); return NULL; }
    sprintf(out, "'%s' %s is not a valid account name: use %s.", key, rep, ALIAS_FORMAT);
    free(rep);
    return out;
}

/* ---- normalize_targets(raw) ---- */
json_t *w19_tool_6f4666_normalize_targets(json_t *raw) {
    json_t *out = json_object();
    json_t *managed = json_array();
    json_t *mcp = json_array();
    json_t *items = raw;
    int free_items = 0;
    size_t i;

    json_set(out, "managed", managed);
    json_set(out, "mcp", mcp);

    if (!raw || raw->type == JSON_NULL) {
        json_set(out, "error", json_null());
        return out;
    }
    if (raw->type == JSON_STRING || raw->type == JSON_OBJECT) {
        items = json_array();
        json_append(items, json_copy(raw));
        free_items = 1;
    } else if (raw->type != JSON_ARRAY) {
        json_set(out, "error",
                 json_string("'connectors' must be a list of names or {name, mcp} objects."));
        return out;
    }

    for (i = 0; i < json_len(items); i++) {
        json_t *item = json_get(items, i);
        char *name = NULL;
        char *alias = NULL;
        char *to = NULL;
        int is_mcp = 0;
        if (item && item->type == JSON_OBJECT) {
            static const char *const known[] = {"name", "mcp", "alias", "to"};
            char *unknown[32];
            size_t nunknown = 0, k;
            for (k = 0; k < json_object_size(item); k++) {
                const char *key = json_object_get_key_at(item, k);
                if (!key || in_list(known, 4, key)) continue;
                if (nunknown < 32) unknown[nunknown++] = dupstr(key);
            }
            if (nunknown) {
                char *joined;
                size_t a, b;
                /* sorted(set(item) - _TARGET_FIELDS) */
                for (a = 1; a < nunknown; a++) {
                    char *keyv = unknown[a];
                    b = a;
                    while (b > 0 && strcmp(unknown[b - 1], keyv) > 0) {
                        unknown[b] = unknown[b - 1];
                        b--;
                    }
                    unknown[b] = keyv;
                }
                joined = join_list((const char *const *)unknown, nunknown);
                const char *suffix =
                    ": a target is {\"name\": \"<slug>\"} or {\"name\": \"<server>\", "
                    "\"mcp\": true}. Transport, URLs and credentials come from the catalog "
                    "manifest, never from the call.";
                char *msg = (char *)malloc(strlen(joined) + strlen(suffix) + 64);
                if (msg) {
                    sprintf(msg, "unknown target field(s) %s%s", joined, suffix);
                    json_set(out, "error", json_string(msg));
                    free(msg);
                }
                free(joined);
                for (k = 0; k < nunknown; k++) free(unknown[k]);
                if (free_items) json_free(items);
                return out;
            }
            {
                json_t *nv = json_obj_get(item, "name");
                json_t *av = json_obj_get(item, "alias");
                json_t *tv = json_obj_get(item, "to");
                json_t *mv = json_obj_get(item, "mcp");
                name = (nv && nv->type == JSON_STRING) ? py_lower_dup(nv->str_val) : dupstr("");
                if (name) {
                    char *stripped = py_strip_dup(name);
                    free(name);
                    name = stripped;
                }
                is_mcp = jtruthy(mv);
                if (is_mcp && (av || tv)) {
                    json_set(out, "error",
                             json_string("'alias' and 'to' name hosted connector accounts; "
                                         "an MCP target takes neither."));
                    free(name);
                    if (free_items) json_free(items);
                    return out;
                }
                if (av && av->type != JSON_NULL)
                    alias = (av->type == JSON_STRING) ? py_strip_dup(av->str_val) : dupstr("");
                if (tv && tv->type != JSON_NULL)
                    to = (tv->type == JSON_STRING) ? py_strip_dup(tv->str_val) : dupstr("");
            }
        } else {
            if (item && item->type == JSON_STRING) {
                char *lowered = py_lower_dup(item->str_val);
                char *stripped = py_strip_dup(lowered);
                free(lowered);
                name = stripped;
            } else {
                name = dupstr("");
            }
            is_mcp = 0;
        }
        if (!name || !name[0]) {
            json_set(out, "error", json_string("every target needs a non-empty 'name'."));
            free(name);
            free(alias);
            free(to);
            if (free_items) json_free(items);
            return out;
        }
        if (is_mcp) {
            int dup = 0;
            size_t k;
            for (k = 0; k < json_len(mcp); k++) {
                json_t *v = json_get(mcp, k);
                if (v && v->type == JSON_STRING && !strcmp(v->str_val, name)) { dup = 1; break; }
            }
            if (!dup) json_append(mcp, json_string(name));
            free(name);
            free(alias);
            free(to);
            continue;
        }
        {
            int dup = 0;
            size_t k;
            for (k = 0; k < json_len(managed); k++) {
                json_t *t = json_get(managed, k);
                const char *tn = jstr_or_null(t, "name");
                const char *ta = jstr_or_null(t, "alias");
                int same_name = tn && name && !strcmp(tn, name);
                int same_alias = (ta && alias) ? !strcmp(ta, alias) : (ta == NULL && alias == NULL);
                if (same_name && same_alias) { dup = 1; break; }
            }
            if (!dup) {
                json_t *target = json_object();
                json_set(target, "name", json_string(name));
                json_set(target, "alias", alias ? json_string(alias) : json_null());
                json_set(target, "to", to ? json_string(to) : json_null());
                json_append(managed, target);
            }
        }
        free(name);
        free(alias);
        free(to);
    }
    if (free_items) json_free(items);
    json_set(out, "error", json_null());
    return out;
}

/* ---- _validate_accounts(action, managed) ---- */
static char *validate_accounts(const char *action, json_t *managed) {
    size_t n = (managed && managed->type == JSON_ARRAY) ? json_len(managed) : 0;
    size_t i;
    int any_to = 0, any_alias = 0;
    for (i = 0; i < n; i++) {
        const char *to = jstr_or_null(json_get(managed, i), "to");
        const char *alias = jstr_or_null(json_get(managed, i), "alias");
        if (to && to[0]) any_to = 1;
        if (alias) any_alias = 1;
    }
    if (strcmp(action, "rename") != 0 && any_to)
        return dupstr("'to' is the new name of an account and applies to action rename only.");
    if (!strcmp(action, "display") && any_alias)
        return dupstr("display takes connector slugs to suggest (e.g. [\"gmail\"]); "
                      "accounts are named on connect.");
    if (!strcmp(action, "rename")) {
        const char *alias = (n == 1) ? jstr_or_null(json_get(managed, 0), "alias") : NULL;
        const char *to = (n == 1) ? jstr_or_null(json_get(managed, 0), "to") : NULL;
        if (n != 1 || !alias || !alias[0] || !to || !to[0])
            return dupstr("rename takes exactly one target {\"name\": \"<slug>\", \"alias\": "
                          "\"<current name>\", \"to\": \"<new name>\"}. Use action status to "
                          "list each connector's accounts.");
        return bad_alias("to", to);
    }
    if (any_alias && n > 1)
        return dupstr("a call with an 'alias' takes that one target only; name other accounts "
                      "in separate calls.");
    if (!strcmp(action, "reconnect")) return NULL;
    for (i = 0; i < n; i++) {
        const char *alias = jstr_or_null(json_get(managed, i), "alias");
        char *err = bad_alias("alias", alias);
        if (err) return err;
    }
    return NULL;
}

/* ---- validate_action(action, managed, mcp) ---- */
json_t *w19_tool_6f4666_validate_action(const char *action, json_t *managed, json_t *mcp) {
    const char *act = action ? action : "";
    size_t nmcp = (mcp && mcp->type == JSON_ARRAY) ? json_len(mcp) : 0;
    size_t nmanaged = (managed && managed->type == JSON_ARRAY) ? json_len(managed) : 0;
    int is_mcp_action = in_list(MCP_ACTIONS, N_ACTIONS(MCP_ACTIONS), act);
    char *err;

    if (!in_list(ALL_ACTIONS, N_ACTIONS(ALL_ACTIONS), act)) {
        char *all = join_list(ALL_ACTIONS, N_ACTIONS(ALL_ACTIONS));
        char *mcpj = join_list(MCP_ACTIONS, N_ACTIONS(MCP_ACTIONS));
        const char *suffix =
            " apply to local MCP servers (targets {\"name\": ..., \"mcp\": true}); "
            "the rest apply to managed connectors. Disconnecting an account is done by the "
            "user in the Nous Portal dashboard, not through this tool.";
        char *msg = (char *)malloc(strlen(all) + strlen(mcpj) + strlen(suffix) + 64);
        json_t *node;
        if (!msg) { free(all); free(mcpj); return json_null(); }
        sprintf(msg, "action must be one of %s. %s%s", all, mcpj, suffix);
        node = json_string(msg);
        free(msg);
        free(all);
        free(mcpj);
        return node;
    }
    if (is_mcp_action) {
        if (nmanaged) {
            size_t i, len = 1;
            char *names, *msg;
            for (i = 0; i < nmanaged; i++) {
                const char *nm = jstr_or_null(json_get(managed, i), "name");
                len += strlen(nm ? nm : "") + 2;
            }
            names = (char *)malloc(len);
            if (!names) return json_null();
            names[0] = '\0';
            for (i = 0; i < nmanaged; i++) {
                const char *nm = jstr_or_null(json_get(managed, i), "name");
                if (i) strcat(names, ", ");
                strcat(names, nm ? nm : "");
            }
            msg = (char *)malloc(strlen(act) + strlen(names) + 200);
            if (!msg) { free(names); return json_null(); }
            sprintf(msg, "'%s' is an MCP action: every target must carry \"mcp\": true "
                         "(got managed connector(s) %s). Managed connectors use connect / "
                         "reconnect / status.", act, names);
            {
                json_t *node = json_string(msg);
                free(msg);
                free(names);
                return node;
            }
        }
        if (!nmcp) {
            char *msg = (char *)malloc(strlen(act) + 160);
            json_t *node;
            if (!msg) return json_null();
            sprintf(msg, "'%s' requires 'connectors': the MCP server name(s), e.g. "
                         "[{\"name\": \"linear\", \"mcp\": true}].", act);
            node = json_string(msg);
            free(msg);
            return node;
        }
    } else if (nmcp) {
        size_t i, len = 1;
        char *names, *mcpj, *msg;
        for (i = 0; i < nmcp; i++) {
            json_t *v = json_get(mcp, i);
            len += strlen((v && v->type == JSON_STRING && v->str_val) ? v->str_val : "") + 2;
        }
        names = (char *)malloc(len);
        if (!names) return json_null();
        names[0] = '\0';
        for (i = 0; i < nmcp; i++) {
            json_t *v = json_get(mcp, i);
            if (i) strcat(names, ", ");
            strcat(names, (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "");
        }
        mcpj = join_list(MCP_ACTIONS, N_ACTIONS(MCP_ACTIONS));
        msg = (char *)malloc(strlen(act) + strlen(names) + strlen(mcpj) + 96);
        if (!msg) { free(names); free(mcpj); return json_null(); }
        sprintf(msg, "'%s' is a managed-connector action; MCP targets (%s) use %s.",
                act, names, mcpj);
        {
            json_t *node = json_string(msg);
            free(msg);
            free(names);
            free(mcpj);
            return node;
        }
    }
    err = validate_accounts(act, managed);
    if (!err) return json_null();
    {
        json_t *node = json_string(err);
        free(err);
        return node;
    }
}

/* ---- manage_connections(args) ---- */
json_t *w19_tool_6f4666_manage_connections(json_t *args) {
    json_t *args_obj = (args && args->type == JSON_OBJECT) ? args : NULL;
    json_t *action_node = args_obj ? json_obj_get(args_obj, "action") : NULL;
    char *action;
    json_t *normalized;
    json_t *error_node;
    json_t *out = NULL;

    /* action = str(args.get("action") or "status").strip().lower() */
    if (action_node && action_node->type == JSON_STRING && action_node->str_val[0])
        action = py_lower_dup(action_node->str_val);
    else if (action_node && action_node->type == JSON_NUMBER && action_node->num_val != 0.0) {
        char num[64];
        snprintf(num, sizeof(num), "%g", action_node->num_val);
        action = py_lower_dup(num);
    } else {
        action = dupstr("status");
    }
    if (!action) return json_null();
    {
        char *stripped = py_strip_dup(action);
        free(action);
        action = stripped;
    }

    normalized = w19_tool_6f4666_normalize_targets(args_obj ? json_obj_get(args_obj, "connectors")
                                                           : NULL);
    error_node = json_obj_get(normalized, "error");
    if (error_node && error_node->type == JSON_STRING) {
        out = json_object();
        json_set(out, "error", json_string(error_node->str_val));
        json_free(normalized);
        free(action);
        return out;
    }
    {
        json_t *managed = json_obj_get(normalized, "managed");
        json_t *mcp = json_obj_get(normalized, "mcp");
        json_t *verr = w19_tool_6f4666_validate_action(action, managed, mcp);
        if (verr && verr->type == JSON_STRING) {
            out = json_object();
            json_set(out, "error", json_string(verr->str_val));
            json_free(verr);
            json_free(normalized);
            free(action);
            return out;
        }
        json_free(verr);
        out = json_object();
        if (in_list(MCP_ACTIONS, N_ACTIONS(MCP_ACTIONS), action)) {
            json_set(out, "kind", json_string("mcp"));
            json_set(out, "action", json_string(action));
            json_set(out, "targets", json_copy(mcp));
        } else {
            json_t *names = json_array();
            size_t i;
            for (i = 0; i < json_len(managed); i++) {
                const char *nm = jstr_or_null(json_get(managed, i), "name");
                json_append(names, json_string(nm ? nm : ""));
            }
            json_set(out, "kind", json_string("managed"));
            json_set(out, "action", json_string(action));
            json_set(out, "targets", names);
        }
    }
    json_free(normalized);
    free(action);
    return out;
}
