/*
 * mcp_tool_discovery.c — C port of upstream Hermes tools/mcp_tool_discovery.py.
 *
 * PoP: w11_mcptooldi_fd99f7_connected_summary @ tools/mcp_tool_discovery.py:_connected_summary
 * PoP: w11_mcptooldi_fd99f7_unregistered @ tools/mcp_tool_discovery.py:_unregistered
 * PoP: w11_mcptooldi_fd99f7_awaiting_connect @ tools/mcp_tool_discovery.py:_awaiting_connect
 * PoP: w11_mcptooldi_fd99f7_not_connected @ tools/mcp_tool_discovery.py:_not_connected
 * PoP: w11_mcptooldi_fd99f7_parallel_safe @ tools/mcp_tool_discovery.py:is_mcp_tool_parallel_safe
 * PoP: w11_mcptooldi_fd99f7_resolve_server_lazy @ tools/mcp_tool_discovery.py:_resolve_server_lazy
 *
 * Origin _core state is injected as data seams from the fixture (single-scope:
 * _server_key / _resolve_server_key are identity), so no live MCP loop runs.
 */
#include "noc_w11_mcptooldi_fd99f7.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MCP_TOOL_NAME_PREFIX "mcp__"

typedef struct { char **v; size_t n, cap; } strvec;

static void sv_push(strvec *s, const char *x) {
    if (!x) return;
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->v = (char **)realloc(s->v, s->cap * sizeof(char *));
    }
    s->v[s->n++] = strdup(x);
}

static void sv_free(strvec *s) {
    for (size_t i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    s->v = NULL; s->n = s->cap = 0;
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *sv_sorted_unique_json(strvec *s) {
    if (!s->n) return json_array();
    qsort(s->v, s->n, sizeof(char *), cmp_str);
    json_t *out = json_array();
    for (size_t i = 0; i < s->n; i++) {
        if (i && !strcmp(s->v[i], s->v[i - 1])) continue;
        json_append(out, json_string(s->v[i]));
    }
    return out;
}

static int obj_has(const json_t *o, const char *k) {
    return json_obj_get(o, k) != NULL;
}

static int arr_has(const json_t *a, const char *s) {
    if (!a || a->type != JSON_ARRAY || !s) return 0;
    for (size_t i = 0; i < json_len(a); i++) {
        json_t *e = json_get(a, i);
        if (e && e->type == JSON_STRING && !strcmp(e->str_val, s)) return 1;
    }
    return 0;
}

/* _server_connect_errors.get(key) or None (empty string is falsy too). */
static const char *error_reason(const json_t *errs, const char *name) {
    json_t *e = json_obj_get(errs, name);
    if (!e || e->type != JSON_STRING || !e->str_val || !*e->str_val) return NULL;
    return e->str_val;
}

static int cooldown_active(const json_t *retry_after, const char *name, double now) {
    json_t *d = json_obj_get(retry_after, name);
    if (!d || d->type != JSON_NUMBER) return 0;
    return now < d->num_val;
}

static int name_in_servers(const json_t *servers, const char *name) {
    return obj_has(servers, name);
}

static int is_unregistered(const char *name, const json_t *servers,
                           const json_t *connecting, const json_t *lazy_configs) {
    return !name_in_servers(servers, name) && !arr_has(connecting, name) &&
           !arr_has(lazy_configs, name);
}

json_t *w11_mcptooldi_fd99f7_connected_summary(const json_t *names,
        const json_t *servers, const json_t *connect_errors,
        long long lazy_tools, long long lazy_servers) {
    long long tool_count = 0, connected = 0;
    json_t *failed = json_array();
    if (names && names->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(names); i++) {
            json_t *ne = json_get(names, i);
            const char *n = (ne && ne->type == JSON_STRING) ? ne->str_val : NULL;
            if (!n) continue;
            const char *reason = error_reason(connect_errors, n);
            int has_err = obj_has(connect_errors, n);
            if (name_in_servers(servers, n) && !has_err) {
                connected++;
                json_t *sv = json_obj_get(servers, n);
                json_t *tc = json_obj_get(sv, "tools");
                if (tc && tc->type == JSON_NUMBER) tool_count += (long long)tc->num_val;
            } else {
                json_t *row = json_array();
                json_append(row, json_string(n));
                json_append(row, json_string(reason ? reason : "not attempted (in retry cooldown)"));
                json_append(failed, row);
            }
        }
    }
    json_t *out = json_object();
    json_set(out, "tool_count", json_number((double)(tool_count + lazy_tools)));
    json_set(out, "connected", json_number((double)(connected + lazy_servers)));
    json_set(out, "failed", failed);
    return out;
}

static json_t *unregistered_json(const json_t *wanted, const json_t *servers,
                                 const json_t *connecting, const json_t *lazy_configs) {
    strvec v = {0};
    if (wanted && wanted->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(wanted); i++) {
            json_t *e = json_get(wanted, i);
            const char *n = (e && e->type == JSON_STRING) ? e->str_val : NULL;
            if (n && is_unregistered(n, servers, connecting, lazy_configs)) sv_push(&v, n);
        }
    }
    json_t *out = sv_sorted_unique_json(&v);
    sv_free(&v);
    return out;
}

json_t *w11_mcptooldi_fd99f7_unregistered(const json_t *wanted, const json_t *servers,
        const json_t *connecting, const json_t *lazy_configs) {
    return unregistered_json(wanted, servers, connecting, lazy_configs);
}

json_t *w11_mcptooldi_fd99f7_awaiting_connect(const json_t *wanted, const json_t *servers,
        const json_t *connecting, const json_t *lazy_configs,
        const json_t *retry_after, double now) {
    json_t *unreg = unregistered_json(wanted, servers, connecting, lazy_configs);
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(unreg); i++) {
        json_t *e = json_get(unreg, i);
        const char *n = e && e->type == JSON_STRING ? e->str_val : NULL;
        if (n && !cooldown_active(retry_after, n, now)) json_append(out, json_copy(e));
    }
    json_free(unreg);
    return out;
}

json_t *w11_mcptooldi_fd99f7_not_connected(const json_t *wanted, const json_t *servers,
        const json_t *connect_errors, const json_t *connecting,
        const json_t *lazy_configs, const json_t *retry_after, double now) {
    (void)retry_after; (void)now; /* cooldown does not gate _not_connected */
    strvec v = {0};
    if (wanted && wanted->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(wanted); i++) {
            json_t *e = json_get(wanted, i);
            const char *n = (e && e->type == JSON_STRING) ? e->str_val : NULL;
            if (!n) continue;
            int add = 0;
            if (is_unregistered(n, servers, connecting, lazy_configs)) add = 1;
            else {
                int has_err = obj_has(connect_errors, n);
                int parked = 0;
                json_t *sv = json_obj_get(servers, n);
                json_t *p = json_obj_get(sv, "park");
                if (p && p->type == JSON_BOOL) parked = p->bool_val ? 1 : 0;
                if ((name_in_servers(servers, n) && (has_err || parked)) ||
                    (arr_has(lazy_configs, n) && has_err)) add = 1;
            }
            if (add) sv_push(&v, n);
        }
    }
    json_t *out = sv_sorted_unique_json(&v);
    sv_free(&v);
    return out;
}

int w11_mcptooldi_fd99f7_parallel_safe(const char *tool_name,
        const json_t *mcp_tool_server_names, const json_t *parallel_safe) {
    if (!tool_name || strncmp(tool_name, MCP_TOOL_NAME_PREFIX, strlen(MCP_TOOL_NAME_PREFIX)))
        return 0;
    const char *srv = json_get_str(mcp_tool_server_names, tool_name, "");
    if (!srv || !*srv) return 0;
    return obj_has(parallel_safe, srv) ? 1 : 0;
}

/* Faithful _parse_boolish with default=False (lazy flag). */
int w11_mcptooldi_fd99f7_resolve_server_lazy(const json_t *config) {
    json_t *v = json_obj_get(config, "lazy");
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) {
        const char *p = v->str_val ? v->str_val : "";
        while (*p && isspace((unsigned char)*p)) p++;
        const char *e = p + strlen(p);
        while (e > p && isspace((unsigned char)e[-1])) e--;
        size_t n = (size_t)(e - p);
        char low[64];
        if (n >= sizeof(low)) return 0;
        for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)p[i]);
        low[n] = '\0';
        const char *trues[] = {"true", "1", "yes", "on"};
        const char *falses[] = {"false", "0", "no", "off"};
        for (int i = 0; i < 4; i++) if (!strcmp(low, trues[i])) return 1;
        for (int i = 0; i < 4; i++) if (!strcmp(low, falses[i])) return 0;
        return 0; /* default */
    }
    return 0;
}
