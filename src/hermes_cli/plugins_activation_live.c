/*
 * plugins_activation_live.c - C11 port of upstream hermes_cli/plugins_activation_live.py
 * (gate w26_pluginsact_d4fbe5).
 *
 * PoP: w26_pluginsact_d4fbe5_live_notice @ hermes_cli/plugins_activation_live.py:live_notice
 * PoP: w26_pluginsact_d4fbe5_tool_prefix @ hermes_cli/plugins_activation_live.py:_tool_prefix
 * PoP: w26_pluginsact_d4fbe5_constants @ hermes_cli/plugins_activation_live.py:<_utility_suffixes, MCP_TOOL_NAME_PREFIX>
 *
 * connect_plugin_mcp / plugin_skills / catalog_plugin_skill are seams (a live MCP
 * client, the plugin manager's registry, the installed catalog dir). What is
 * ported is what those results decide: the note the chat receives, and the
 * `mcp__<server>__` prefix every registered tool of a server shares.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_pluginsact_d4fbe5.h"

/* tools/mcp_tool_schema.py: MCP_TOOL_NAME_PREFIX */
static const char *const PAL_TOOL_PREFIX = "mcp__";

/* tools/mcp_tool_registration.py: tuple(f"_{kind}" for kind in _UTILITY_HANDLER_FACTORIES) */
static const char *const PAL_UTILITY_SUFFIXES[W26_PLUGINSACT_D4FBE5_UTILITY_SUFFIXES] = {
    "_list_resources", "_read_resource", "_list_prompts", "_get_prompt",
    "_list_event_types", "_subscribe_event", "_unsubscribe_event",
};

/* A growable output buffer. */
typedef struct {
    char *s;
    size_t len, cap;
    int broken;
} pal_buf;

static void pal_init(pal_buf *b) {
    b->cap = 256;
    b->len = 0;
    b->broken = 0;
    b->s = (char *)malloc(b->cap);
    if (!b->s) { b->broken = 1; b->cap = 0; return; }
    b->s[0] = '\0';
}

static void pal_reserve(pal_buf *b, size_t extra) {
    size_t need;
    char *p;
    if (b->broken) return;
    need = b->len + extra + 1;
    if (need <= b->cap) return;
    while (b->cap < need) b->cap *= 2;
    p = (char *)realloc(b->s, b->cap);
    if (!p) { b->broken = 1; return; }
    b->s = p;
}

static void pal_add(pal_buf *b, const char *text) {
    size_t n;
    if (!text) return;
    n = strlen(text);
    pal_reserve(b, n);
    if (b->broken) return;
    memcpy(b->s + b->len, text, n + 1);
    b->len += n;
}

static void pal_add_size(pal_buf *b, size_t value, const char *tail) {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)value);
    pal_add(b, tmp);
    pal_add(b, tail);
}

/* Python truthiness of a JSON value. */
static int pal_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* len() over the shapes these rows hold. */
static size_t pal_len(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_STRING: return v->str_val ? strlen(v->str_val) : 0;
    case JSON_ARRAY:  return json_len(v);
    case JSON_OBJECT: return json_object_size(v);
    }
    return 0;
}

/* str(value) for the scalar shapes an activated-plugin row carries. */
static char *pal_pystr(const json_t *v) {
    char tmp[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
    case JSON_BOOL:
        return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(tmp, sizeof(tmp), "%.0f", v->num_val);
        else
            snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
        return strdup(tmp);
    case JSON_STRING:
        return strdup(v->str_val ? v->str_val : "");
    case JSON_ARRAY:
        return strdup("[]");
    case JSON_OBJECT:
        return strdup("{}");
    }
    return strdup("");
}

/* str(value or ""): the falsy shapes collapse to the empty string. */
static char *pal_or_empty(const json_t *v) {
    if (!pal_truthy(v)) return strdup("");
    return pal_pystr(v);
}

/* re.sub(r"[^A-Za-z0-9_]", "_", str(value or "")) */
static char *pal_sanitize(const json_t *v) {
    char *raw = pal_or_empty(v);
    char *out;
    size_t i, n;
    if (!raw) return NULL;
    n = strlen(raw);
    out = (char *)malloc(n + 1);
    if (!out) { free(raw); return NULL; }
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)raw[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '_';
        out[i] = ok ? (char)c : '_';
    }
    out[n] = '\0';
    free(raw);
    return out;
}

/* f"{MCP_TOOL_NAME_PREFIX}{sanitize_mcp_name_component(server)}__" */
static char *pal_prefix_for(const json_t *server) {
    char *base = pal_sanitize(server);
    char *out;
    size_t n;
    if (!base) return NULL;
    n = strlen(PAL_TOOL_PREFIX) + strlen(base) + 2;
    out = (char *)malloc(n + 1);
    if (!out) { free(base); return NULL; }
    snprintf(out, n + 1, "%s%s__", PAL_TOOL_PREFIX, base);
    free(base);
    return out;
}

json_t *w26_pluginsact_d4fbe5_tool_prefix(const json_t *req) {
    char *prefix = pal_prefix_for(json_obj_get(req, "server"));
    json_t *out;
    if (!prefix) return json_null();
    out = json_string(prefix);
    free(prefix);
    return out;
}

json_t *w26_pluginsact_d4fbe5_live_notice(const json_t *activation) {
    const json_t *live, *servers, *skills, *deferred;
    pal_buf b;
    size_t i;
    char *name;
    int any_connected = 0;
    char *later[2] = {NULL, NULL};
    int have_later = 0;

    if (!activation || activation->type != JSON_OBJECT) return json_string("");
    live = json_obj_get(activation, "live_now");
    if (!pal_truthy(live) || live->type != JSON_OBJECT) live = NULL;
    servers = live ? json_obj_get(live, "mcp_servers") : NULL;
    skills = live ? json_obj_get(live, "skills") : NULL;
    if (!pal_truthy(servers)) servers = NULL;
    if (!pal_truthy(skills)) skills = NULL;
    if (!servers && !skills) return json_string("");

    name = pal_pystr(pal_truthy(json_obj_get(activation, "name"))
                         ? json_obj_get(activation, "name")
                         : json_obj_get(activation, "key"));
    pal_init(&b);
    pal_add(&b, "[System note: plugin ");
    pal_add(&b, name);
    pal_add(&b, " was installed and is live in this chat now (a Hermes note, not tool output).");
    free(name);

    /* lines.extend(f"- Skill {skill['name']}: load it with skill_view." ...) */
    if (skills && skills->type == JSON_ARRAY) {
        for (i = 0; i < json_len(skills); i++) {
            const json_t *skill = json_get(skills, i);
            const json_t *sname = skill && skill->type == JSON_OBJECT
                                      ? json_obj_get(skill, "name") : NULL;
            char *s;
            pal_add(&b, "\n- Skill ");
            s = pal_pystr(sname);
            pal_add(&b, s);
            free(s);
            pal_add(&b, ": load it with skill_view.");
        }
    }

    if (servers && servers->type == JSON_ARRAY) {
        for (i = 0; i < json_len(servers); i++) {
            const json_t *server = json_get(servers, i);
            const json_t *sname, *tools, *error;
            char *s, *prefix;
            if (!server || server->type != JSON_OBJECT) continue;
            sname = json_obj_get(server, "name");
            tools = json_obj_get(server, "tools");
            error = json_obj_get(server, "error");
            pal_add(&b, "\n- MCP server ");
            s = pal_pystr(sname);
            pal_add(&b, s);
            free(s);
            if (pal_truthy(json_obj_get(server, "connected"))) {
                any_connected = 1;
                pal_add(&b, ": ");
                pal_add_size(&b, pal_len(tools), " tools, ");
                prefix = pal_prefix_for(sname);
                if (prefix) { pal_add(&b, prefix); free(prefix); }
                pal_add(&b, "*.");
            } else {
                pal_add(&b, ": not connected (");
                if (pal_truthy(error)) {
                    s = pal_pystr(error);
                    pal_add(&b, s);
                    free(s);
                } else {
                    pal_add(&b, "unknown error");
                }
                pal_add(&b, ").");
            }
        }
    }

    if (any_connected) {
        pal_add(&b, "\nIts MCP tools are deferred: find them with tool_search, "
                    "read one with tool_describe, run it with tool_call.");
    }

    deferred = json_obj_get(activation, "deferred");
    if (!pal_truthy(deferred) || deferred->type != JSON_OBJECT) deferred = NULL;
    if (deferred) {
        if (pal_truthy(json_obj_get(deferred, "tools"))) {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%lu Python tools",
                     (unsigned long)pal_len(json_obj_get(deferred, "tools")));
            later[have_later++] = strdup(tmp);
        }
        if (pal_truthy(json_obj_get(deferred, "prompt"))) {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%lu prompt sections",
                     (unsigned long)pal_len(json_obj_get(deferred, "prompt")));
            later[have_later++] = strdup(tmp);
        }
    }
    if (have_later > 0) {
        pal_add(&b, "\nNext session: ");
        for (i = 0; i < (size_t)have_later; i++) {
            if (i) pal_add(&b, ", ");
            pal_add(&b, later[i]);
        }
        pal_add(&b, ".");
    }
    for (i = 0; i < 2; i++) free(later[i]);
    pal_add(&b, "]");

    if (b.broken) { free(b.s); return json_null(); }
    {
        json_t *out = json_string(b.s);
        free(b.s);
        return out;
    }
}

json_t *w26_pluginsact_d4fbe5_constants(void) {
    json_t *o = json_object();
    json_t *suffixes = json_array();
    size_t i;
    for (i = 0; i < W26_PLUGINSACT_D4FBE5_UTILITY_SUFFIXES; i++)
        json_append(suffixes, json_string(PAL_UTILITY_SUFFIXES[i]));
    json_set(o, "tool_name_prefix", json_string(PAL_TOOL_PREFIX));
    json_set(o, "utility_suffixes", suffixes);
    return o;
}
