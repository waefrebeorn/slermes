/*
 * plugins_cmd_capabilities.c - C11 port of upstream
 * hermes_cli/plugins_cmd_capabilities.py (gate w26_pluginscmd_1f9092).
 *
 * PoP: w26_pluginscmd_1f9092_declared @ hermes_cli/plugins_cmd_capabilities.py:_declared_capabilities_from_manifest
 * PoP: w26_pluginscmd_1f9092_known @ hermes_cli/plugin_capabilities.py:_known
 * PoP: w26_pluginscmd_1f9092_constants @ hermes_cli/plugin_capabilities.py:_CAPABILITY_ROWS
 *
 * The consent screen prints, the config write and the manifest read are seams.
 * What is ported is the normalization that decides which declared capabilities
 * survive, the registry filter the effective set is computed with, and the
 * registry table itself (id, deprecated legacy gate, one-line risk text) that
 * the marker of "declared vs granted" is chosen from.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_pluginscmd_1f9092.h"

typedef struct {
    const char *id;
    const char *legacy;      /* deprecated plugins.entries.<id> path, "" when none */
    const char *description;
} pc_capability;

/* _CAPABILITY_ROWS, in declaration order. */
static const pc_capability PC_CAPS[W26_PLUGINSCMD_1F9092_CAPABILITY_COUNT] = {
    {"tools.override", "allow_tool_override",
     "Replace built-in tools (e.g. shell_exec, write_file) — an "
     "override can intercept everything routed through that tool"},
    {"llm.provider_override", "llm,allow_provider_override",
     "Run host-owned LLM calls against a provider other than your "
     "active one (uses your credentials)"},
    {"llm.model_override", "llm,allow_model_override",
     "Choose which model host-owned LLM calls use (spend follows "
     "the chosen model)"},
    {"llm.agent_id_override", "llm,allow_agent_id_override",
     "Attribute its LLM calls to a different agent id"},
    {"llm.profile_override", "llm,allow_profile_override",
     "Run LLM calls under a different auth profile"},
    {"llm.task_override", "llm,allow_task_override",
     "Route its LLM calls through the host's built-in auxiliary "
     "task lanes"},
    {"llm.side_question", "",
     "Ask your agent a short question after each turn. It sees the "
     "whole session, including memory, and spends your main model's tokens."},
    {"llm.session_fork", "",
     "Run a background copy of your agent after a turn. It sees the whole session, including "
     "memory, can use the plugin's own tools, and spends your main model's tokens."},
    {"gateway.platform_actions", "allow_platform_actions",
     "Act on connected chat platforms as the gateway bot "
     "(add reactions, rename threads) via ctx.platform_actions"},
};

static int pc_is_valid(const char *id) {
    size_t i;
    if (!id) return 0;
    for (i = 0; i < W26_PLUGINSCMD_1F9092_CAPABILITY_COUNT; i++)
        if (!strcmp(PC_CAPS[i].id, id)) return 1;
    return 0;
}

/* Python str.strip() over ASCII whitespace; NULL when out of memory. */
static char *pc_strip(const char *s) {
    size_t n, a = 0, b;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    while (a < n && isspace((unsigned char)s[a])) a++;
    b = n;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* Python truthiness of a JSON value. */
static int pc_truthy(const json_t *v) {
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

static int pc_array_has(const json_t *arr, const char *id) {
    size_t i, n = json_len(arr);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, id)) return 1;
    }
    return 0;
}

json_t *w26_pluginscmd_1f9092_declared(const json_t *req) {
    const json_t *manifest = req ? json_obj_get(req, "manifest") : NULL;
    const json_t *raw = NULL;
    json_t *out = json_array();
    size_t i, n;

    if (manifest && manifest->type == JSON_OBJECT) raw = json_obj_get(manifest, "capabilities");
    if (!pc_truthy(raw)) return out;              /* if not raw: return [] */
    if (raw->type != JSON_ARRAY) return out;      /* not a list -> ignore */
    n = json_len(raw);
    for (i = 0; i < n; i++) {
        const json_t *item = json_get(raw, i);
        char *cap;
        if (!item || item->type != JSON_STRING) continue;   /* non-string entry */
        cap = pc_strip(item->str_val);
        if (!cap) continue;
        if (pc_is_valid(cap) && !pc_array_has(out, cap)) json_append(out, json_string(cap));
        free(cap);
    }
    return out;
}

json_t *w26_pluginscmd_1f9092_known(const json_t *req) {
    const json_t *caps = req ? json_obj_get(req, "capabilities") : NULL;
    json_t *out = json_array();
    size_t i, n;

    if (!caps || caps->type != JSON_ARRAY) return out;
    n = json_len(caps);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(caps, i);
        if (!e || e->type != JSON_STRING || !e->str_val) continue;
        if (pc_is_valid(e->str_val) && !pc_array_has(out, e->str_val))
            json_append(out, json_string(e->str_val));
    }
    return out;
}

json_t *w26_pluginscmd_1f9092_constants(void) {
    json_t *o = json_object();
    json_t *rows = json_array();
    size_t i;
    for (i = 0; i < W26_PLUGINSCMD_1F9092_CAPABILITY_COUNT; i++) {
        json_t *row = json_object();
        json_t *legacy = json_array();
        const char *p = PC_CAPS[i].legacy;
        json_set(row, "id", json_string(PC_CAPS[i].id));
        while (p && *p) {                       /* (\"llm\", \"allow_x\") -> [\"llm\", \"allow_x\"] */
            const char *comma = strchr(p, ',');
            size_t len = comma ? (size_t)(comma - p) : strlen(p);
            char *part = (char *)malloc(len + 1);
            if (!part) break;
            memcpy(part, p, len);
            part[len] = '\0';
            json_append(legacy, json_string(part));
            free(part);
            p = comma ? comma + 1 : NULL;
        }
        json_set(row, "legacy_path", legacy);
        json_set(row, "description", json_string(PC_CAPS[i].description));
        json_append(rows, row);
    }
    json_set(o, "capabilities", rows);
    json_set(o, "granted_key", json_string("granted_capabilities"));
    json_set(o, "consent_key", json_string("capabilities_consent"));
    return o;
}
