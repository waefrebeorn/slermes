/*
 * delegate_tool_toolsets.c - C home for upstream Hermes tools/delegate_tool_toolsets.py.
 *
 * PoP: w16_delegateto_7a3a62_is_mcp_toolset_name @ tools/delegate_tool_toolsets.py:_is_mcp_toolset_name
 * PoP: w16_delegateto_7a3a62_blocked_toolsets_for_role @ tools/delegate_tool_toolsets.py:_blocked_toolsets_for_role
 * PoP: w16_delegateto_7a3a62_strip_blocked_tools @ tools/delegate_tool_toolsets.py:_strip_blocked_tools
 * PoP: w16_delegateto_7a3a62_expand_parent_toolsets @ tools/delegate_tool_toolsets.py:_expand_parent_toolsets
 *
 * The module-level static tables (TOOLSETS, resolve_toolset results and the
 * registry alias target) are passed in from the fixture; the port reproduces
 * the exact Python decision logic over those inputs.
 */
#include "noc_w16_delegateto_7a3a62.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DELEGATE_BLOCKED_TOOLS (frozenset) mirrored verbatim. */
static const char *BLOCKED[] = {
    "delegate_task", "clarify", "memory", "send_message", "cronjob_manage", NULL
};

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}

static int in_blocked(const char *s) {
    if (!s) return 0;
    for (int i = 0; BLOCKED[i]; ++i) if (!strcmp(s, BLOCKED[i])) return 1;
    return 0;
}

/* Minimal sorted string set. */
typedef struct { char **v; size_t n, cap; } w16dtt_strset;
static void ss_add(w16dtt_strset *s, const char *x) {
    if (!x) return;
    for (size_t i = 0; i < s->n; ++i) if (!strcmp(s->v[i], x)) return;
    if (s->n == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 8;
        char **nv = (char **)realloc(s->v, nc * sizeof(char *));
        if (!nv) return;
        s->v = nv; s->cap = nc;
    }
    size_t L = strlen(x) + 1;
    s->v[s->n] = (char *)malloc(L);
    if (!s->v[s->n]) return;
    memcpy(s->v[s->n], x, L);
    s->n++;
}
static int ss_has(const w16dtt_strset *s, const char *x) {
    if (!x) return 0;
    for (size_t i = 0; i < s->n; ++i) if (!strcmp(s->v[i], x)) return 1;
    return 0;
}
static void ss_free(w16dtt_strset *s) {
    for (size_t i = 0; i < s->n; ++i) free(s->v[i]);
    free(s->v); s->v = NULL; s->n = s->cap = 0;
}
static int cmpstr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static json_t *ss_sorted_json(w16dtt_strset *s) {
    json_t *out = json_array();
    if (s->n > 1) qsort(s->v, s->n, sizeof(char *), cmpstr);
    for (size_t i = 0; i < s->n; ++i) json_append(out, json_string(s->v[i]));
    return out;
}

/* Resolve a toolset name from the fixture's resolved map. */
static const json_t *resolve(const json_t *resolved, const char *name) {
    json_t *v = json_obj_get(resolved, name);
    return (v && v->type == JSON_ARRAY) ? v : NULL;
}

/* True when every tool in the array is a member of the given set. */
static int tools_subset_of(const json_t *tools, const w16dtt_strset *allowed) {
    if (!tools || tools->type != JSON_ARRAY || json_len(tools) == 0) return 0;
    for (size_t i = 0; i < json_len(tools); ++i) {
        if (!ss_has(allowed, jstr(json_get(tools, i)))) return 0;
    }
    return 1;
}

json_t *w16_delegateto_7a3a62_is_mcp_toolset_name(const char *name, const char *alias_target) {
    if (!name || !*name) return json_bool(false);
    if (!strncmp(name, "mcp-", 4)) return json_bool(true);
    return json_bool(alias_target && !strncmp(alias_target, "mcp-", 4));
}

json_t *w16_delegateto_7a3a62_blocked_toolsets_for_role(const char *role, const json_t *toolsets) {
    w16dtt_strset blocked = {0};
    for (int i = 0; BLOCKED[i]; ++i) ss_add(&blocked, BLOCKED[i]);
    if (role && !strcmp(role, "orchestrator")) {
        /* discard("delegate_task") - the one blocked tool granted to orchestrators */
        w16dtt_strset nb = {0};
        for (size_t i = 0; i < blocked.n; ++i)
            if (strcmp(blocked.v[i], "delegate_task")) ss_add(&nb, blocked.v[i]);
        ss_free(&blocked); blocked = nb;
    }
    w16dtt_strset out = {0};
    if (toolsets && toolsets->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(toolsets); ++i) {
            const char *name = json_object_get_key_at(toolsets, i);
            json_t *defn = json_object_get_at(toolsets, i);
            json_t *tools = json_obj_get(defn, "tools");
            if (!tools || tools->type != JSON_ARRAY)
                tools = (defn && defn->type == JSON_ARRAY) ? defn : NULL;
            if (tools_subset_of(tools, &blocked)) ss_add(&out, name);
        }
    }
    ss_free(&blocked);
    json_t *r = ss_sorted_json(&out);
    ss_free(&out);
    return r;
}

json_t *w16_delegateto_7a3a62_strip_blocked_tools(const json_t *toolsets, const json_t *resolved) {
    w16dtt_strset banned = {0}, blocked = {0};
    for (int i = 0; BLOCKED[i]; ++i) ss_add(&blocked, BLOCKED[i]);
    ss_add(&banned, "delegation"); ss_add(&banned, "kanban");
    if (resolved && resolved->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(resolved); ++i) {
            const char *name = json_object_get_key_at(resolved, i);
            json_t *tools = json_object_get_at(resolved, i);
            if (tools_subset_of(tools, &blocked)) ss_add(&banned, name);
        }
    }
    json_t *out = json_array();
    if (toolsets && toolsets->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(toolsets); ++i) {
            const char *t = jstr(json_get(toolsets, i));
            if (!ss_has(&banned, t)) json_append(out, json_string(t));
        }
    }
    ss_free(&banned);
    ss_free(&blocked);
    return out;
}

json_t *w16_delegateto_7a3a62_expand_parent_toolsets(const json_t *parent, const json_t *resolved) {
    w16dtt_strset parent_names = {0};
    if (parent && parent->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(parent); ++i) {
            const char *ts = jstr(json_get(parent, i));
            const json_t *tools = resolve(resolved, ts);
            if (tools) for (size_t j = 0; j < json_len(tools); ++j)
                ss_add(&parent_names, jstr(json_get(tools, j)));
        }
    }
    w16dtt_strset expanded = {0};
    if (parent && parent->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(parent); ++i)
            ss_add(&expanded, jstr(json_get(parent, i)));
    if (parent_names.n && resolved && resolved->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(resolved); ++i) {
            const char *name = json_object_get_key_at(resolved, i);
            if (ss_has(&expanded, name)) continue;
            const json_t *tools = json_object_get_at(resolved, i);
            if (tools && tools->type == JSON_ARRAY && json_len(tools) &&
                tools_subset_of(tools, &parent_names))
                ss_add(&expanded, name);
        }
    }
    ss_free(&parent_names);
    json_t *out = ss_sorted_json(&expanded);
    ss_free(&expanded);
    return out;
}
