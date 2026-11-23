/*
 * mcp_tool_agent.c — faithful C11 port of the pure tool-list helpers in
 * tools/mcp_tool_agent.py (gate w7_mcptoolage_9bc44f).
 *
 * PoP: w7_mcptoolage_9bc44f_mcptagent_def_name @ tools/mcp_tool_agent.py:_def_name
 * PoP: w7_mcptoolage_9bc44f_mcptagent_agent_tool_names @ tools/mcp_tool_agent.py:agent_tool_names
 * PoP: w7_mcptoolage_9bc44f_mcptagent_merge_preserving_prefix @ tools/mcp_tool_agent.py:_merge_preserving_prefix
 *
 * Only the pure helpers needing no live agent/registry are ported.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w7_mcptoolage_9bc44f.h"

static int is_type(const json_t *v, json_type_t t) { return v && v->type == t; }

/* (tool_def.get("function") or {}).get("name", "") */
static const char *def_name(const json_t *tool_def) {
    if (!is_type(tool_def, JSON_OBJECT)) return "";
    const json_t *fn = json_obj_get(tool_def, "function");
    if (!is_type(fn, JSON_OBJECT)) return "";
    const json_t *nm = json_obj_get(fn, "name");
    if (!is_type(nm, JSON_STRING)) return "";
    return nm->str_val ? nm->str_val : "";
}

static int in_array(const json_t *arr, const char *s) {
    if (!is_type(arr, JSON_ARRAY) || !s || !*s) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (is_type(v, JSON_STRING) && v->str_val && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

static int is_bridge(const char *name) {
    return name && (!strcmp(name, "tool_search") || !strcmp(name, "tool_describe") ||
                    !strcmp(name, "tool_call"));
}

struct json_t *mcptagent_def_name(const struct json_t *tool_def) {
    return json_string(def_name(tool_def));
}

struct json_t *mcptagent_agent_tool_names(const struct json_t *tools) {
    json_t *out = json_array();
    if (!is_type(tools, JSON_ARRAY)) return out;
    for (size_t i = 0; i < json_len(tools); i++) {
        const char *nm = def_name(json_get(tools, i));
        if (nm && *nm) json_append(out, json_string(nm));
    }
    return out;
}

typedef struct { const char *name; json_t *entry; } Fresh;

static int fresh_find(Fresh *f, size_t n, const char *name) {
    for (size_t i = 0; i < n; i++) if (!strcmp(f[i].name, name)) return (int)i;
    return -1;
}

struct json_t *mcptagent_merge_preserving_prefix(const struct json_t *current_defs,
                                                 const struct json_t *new_defs,
                                                 const struct json_t *registered_names,
                                                 const struct json_t *held) {
    /* fresh = {_def_name(e): e for e in new_defs if _def_name(e)} */
    Fresh *fresh = NULL; size_t fn = 0, fcap = 0;
    if (is_type(new_defs, JSON_ARRAY)) {
        for (size_t i = 0; i < json_len(new_defs); i++) {
            const json_t *e = json_get(new_defs, i);
            const char *nm = def_name(e);
            if (!nm || !*nm) continue;
            int idx = fresh_find(fresh, fn, nm);
            if (idx >= 0) {
                fresh[idx].entry = (json_t *)e; /* dict assignment: value replaced */
            } else {
                if (fn == fcap) {
                    fcap = fcap ? fcap * 2 : 8;
                    Fresh *tmp = (Fresh *)realloc(fresh, fcap * sizeof(Fresh));
                    if (!tmp) break;
                    fresh = tmp;
                }
                fresh[fn].name = nm; fresh[fn].entry = (json_t *)e; fn++;
            }
        }
    }

    json_t *merged = json_array();
    if (is_type(current_defs, JSON_ARRAY)) {
        for (size_t i = 0; i < json_len(current_defs); i++) {
            const json_t *e = json_get(current_defs, i);
            const char *nm = def_name(e);
            int idx = nm && *nm ? fresh_find(fresh, fn, nm) : -1;
            int replacement = (idx >= 0);
            if (idx >= 0) {
                /* fresh.pop(name, None) */
                for (size_t k = (size_t)idx; k + 1 < fn; k++) fresh[k] = fresh[k + 1];
                fn--;
            }
            if (replacement || is_bridge(nm) || (nm && *nm && in_array(registered_names, nm)))
                json_append(merged, json_copy(e));
        }
    }

    /* merged.extend(entry for name, entry in fresh.items() if name not in held) */
    for (size_t i = 0; i < fn; i++) {
        if (!in_array(held, fresh[i].name))
            json_append(merged, json_copy(fresh[i].entry));
    }
    free(fresh);

    /* names = {_def_name(t) for t in merged}, deduped + sorted for JSON */
    json_t *uniq = json_array();
    for (size_t i = 0; i < json_len(merged); i++) {
        const char *nm = def_name(json_get(merged, i));
        if (!nm || !*nm) continue;
        int seen = 0;
        for (size_t j = 0; j < json_len(uniq); j++)
            if (!strcmp(json_string_value(json_get(uniq, j)), nm)) { seen = 1; break; }
        if (!seen) json_append(uniq, json_string(nm));
    }
    for (size_t i = 0; i + 1 < json_len(uniq); i++)
        for (size_t j = 0; j + 1 + i < json_len(uniq); j++)
            if (strcmp(json_string_value(json_get(uniq, j)),
                       json_string_value(json_get(uniq, j + 1))) > 0) {
                json_t *a = json_copy(json_get(uniq, j));
                json_t *b = json_copy(json_get(uniq, j + 1));
                json_array_set(uniq, j, b);
                json_array_set(uniq, j + 1, a);
            }

    json_t *out = json_object();
    json_set(out, "merged", merged);
    json_set(out, "names", uniq);
    return out;
}
