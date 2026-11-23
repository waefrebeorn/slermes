/*
 * tools_config_mcp.c — C home for upstream Hermes hermes_cli/tools_config_mcp.py.
 *
 * PoP: w15_toolsconfi_129c58_preselected @ hermes_cli/tools_config_mcp.py:_mcp_preselected
 * PoP: w15_toolsconfi_129c58_apply_mcp_change @ hermes_cli/tools_config_mcp.py:_apply_mcp_change
 */
#include "hermes_json.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "noc_w15_toolsconfi_129c58.h"

#define CAP 4096

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

/* fnmatch.fnmatchcase, recursive `* ? [seq] [!seq]` matcher. */
static int glob_here(const char *s, const char *p) {
    if (!*p) return *s == '\0';
    if (*p == '*') {
        while (*p == '*') p++;
        if (!*p) return 1;
        for (const char *c = s;; c++) {
            if (glob_here(c, p)) return 1;
            if (!*c) break;
        }
        return 0;
    }
    if (*p == '?') return *s ? glob_here(s + 1, p + 1) : 0;
    if (*p == '[') {
        const char *q = p + 1;
        int neg = 0;
        if (*q == '!' || *q == '^') { neg = 1; q++; }
        int matched = 0, first = 1;
        char prev = 0;
        int have_prev = 0;
        while (*q && *q != ']') {
            if (*q == '-' && have_prev && q[1] && q[1] != ']') {
                char hi = q[1];
                if (*s && (unsigned char)*s >= (unsigned char)prev && (unsigned char)*s <= (unsigned char)hi)
                    matched = 1;
                q += 2; have_prev = 0; first = 0;
                continue;
            }
            if (*s == *q) matched = 1;
            prev = *q; have_prev = 1; first = 0;
            q++;
        }
        if (*q != ']') {
            /* Unmatched '[' is literal in fnmatch. */
            return (*s == '[') ? glob_here(s + 1, p + 1) : 0;
        }
        if (!*s) return 0;
        (void)first;
        if (neg) matched = !matched;
        return matched ? glob_here(s + 1, q + 1) : 0;
    }
    if (*s != *p) return 0;
    return glob_here(s + 1, p + 1);
}

static int has_glob(const char *p) {
    return strpbrk(p, "*?[") != NULL;
}

/* tools.mcp_tool_schema.matches_name_filter semantics. */
static int matches_name_filter(const char *name, const json_t *patterns) {
    if (!patterns || patterns->type != JSON_ARRAY || patterns->c.count == 0) return 0;
    for (size_t i = 0; i < patterns->c.count; i++) {
        json_t *v = json_get(patterns, i);
        if (!v || v->type != JSON_STRING || !v->str_val) continue;
        if (!strcmp(name, v->str_val)) return 1;
    }
    for (size_t i = 0; i < patterns->c.count; i++) {
        json_t *v = json_get(patterns, i);
        if (!v || v->type != JSON_STRING || !v->str_val) continue;
        if (has_glob(v->str_val) && glob_here(name, v->str_val)) return 1;
    }
    return 0;
}

json_t *w15_toolsconfi_129c58_preselected(const json_t *tool_names,
                                         const json_t *include_set,
                                         const json_t *exclude_set) {
    json_t *out = json_array();
    size_t n = (tool_names && tool_names->type == JSON_ARRAY) ? tool_names->c.count : 0;
    int include_mode = include_set && include_set->type != JSON_NULL;
    int exclude_mode = exclude_set && exclude_set->type == JSON_ARRAY && exclude_set->c.count > 0;
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(tool_names, i);
        const char *tn = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
        int take;
        if (include_mode) take = matches_name_filter(tn, include_set);
        else if (exclude_mode) take = !matches_name_filter(tn, exclude_set);
        else take = 1;
        if (take) json_append(out, json_number((double)i));
    }
    return out;
}

static int arr_contains_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

static void arr_remove_str(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 0; i < arr->c.count; ) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) {
            json_array_remove(arr, i);
        } else i++;
    }
}

json_t *w15_toolsconfi_129c58_apply_mcp_change(const json_t *config,
                                              const json_t *targets,
                                              const char *action) {
    json_t *cfg = config && config->type == JSON_OBJECT ? json_copy(config) : json_object();
    json_t *mcp = json_obj_get(cfg, "mcp_servers");
    json_t *failed = json_array();
    int disable = action && !strcmp(action, "disable");
    size_t nt = (targets && targets->type == JSON_ARRAY) ? targets->c.count : 0;
    for (size_t i = 0; i < nt; i++) {
        json_t *tv = json_get(targets, i);
        const char *target = (tv && tv->type == JSON_STRING && tv->str_val) ? tv->str_val : "";
        const char *colon = strchr(target, ':');
        char server[CAP], tool[CAP];
        if (colon) {
            size_t sl = (size_t)(colon - target);
            if (sl >= sizeof(server)) sl = sizeof(server) - 1;
            memcpy(server, target, sl); server[sl] = '\0';
            snprintf(tool, sizeof(tool), "%s", colon + 1);
        } else {
            snprintf(server, sizeof(server), "%s", target);
            tool[0] = '\0';
        }
        json_t *srv = (mcp && mcp->type == JSON_OBJECT) ? json_obj_get(mcp, server) : NULL;
        if (!srv || srv->type != JSON_OBJECT) {
            json_append(failed, json_string(server));
            continue;
        }
        json_t *tools = json_obj_get(srv, "tools");
        if (!tools || tools->type != JSON_OBJECT) {
            tools = json_object();
            json_set(srv, "tools", tools);
        }
        json_t *existing = json_obj_get(tools, "exclude");
        json_t *exclude = (existing && existing->type == JSON_ARRAY) ? json_copy(existing) : json_array();
        if (disable) {
            if (tool[0] && !arr_contains_str(exclude, tool)) json_append(exclude, json_string(tool));
        } else {
            if (tool[0]) arr_remove_str(exclude, tool);
        }
        json_set(tools, "exclude", exclude);
    }
    json_t *out = json_object();
    json_set(out, "failed", failed);
    json_set(out, "config", cfg);
    return out;
}

void hermes_cli_tools_config_mcp_port_pending(void) { (void)0; }

