/*
 * kanban_db_graph.c — C port of upstream Hermes hermes_cli/kanban_db_graph.py.
 *
 * PoP: w13_kanbandbg_30158b_initial_state @ hermes_cli/kanban_db_graph.py:initial_task_state
 * PoP: w13_kanbandbg_30158b_validate_graph @ hermes_cli/kanban_db_graph.py:_validate_children_graph
 *
 * DB-free seams: callers supply the parent rows instead of a sqlite connection.
 */
#include "noc_w13_kanbandbg_30158b.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *err_obj(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    json_t *o = json_object();
    json_set(o, "error", json_string(buf));
    return o;
}

json_t *w13_kanbandbg_30158b_initial_state(const struct json_t *parents,
                                           const struct json_t *rows,
                                           const char *initial_status,
                                           int triage,
                                           const struct json_t *tenant) {
    size_t np = (parents && parents->type == JSON_ARRAY) ? parents->c.count : 0;
    const char **missing = (const char **)calloc(np ? np : 1, sizeof(char *));
    size_t nm = 0;
    json_t **prows = (json_t **)calloc(np ? np : 1, sizeof(json_t *));
    if (!missing || !prows) { free(missing); free(prows); return err_obj("out of memory"); }
    for (size_t i = 0; i < np; i++) {
        const char *pid = jstr(json_get(parents, i));
        json_t *row = (pid && rows) ? json_obj_get(rows, pid) : NULL;
        if (!row || row->type != JSON_OBJECT) {
            missing[nm++] = pid ? pid : "";
            prows[i] = NULL;
        } else {
            prows[i] = row;
        }
    }
    if (nm) {
        char joined[2048];
        size_t at = 0;
        joined[0] = '\0';
        for (size_t i = 0; i < nm; i++) {
            size_t l = strlen(missing[i]);
            if (at + l + 3 >= sizeof(joined)) break;
            if (at) { joined[at++] = ','; joined[at++] = ' '; }
            memcpy(joined + at, missing[i], l);
            at += l;
            joined[at] = '\0';
        }
        free(missing);
        free(prows);
        return err_obj("unknown parent task(s): %s", joined);
    }
    free(missing);
    json_t *resolved_tenant = NULL;
    if (!tenant || tenant->type == JSON_NULL) {
        for (size_t i = 0; i < np; i++) {
            json_t *t = json_obj_get(prows[i], "tenant");
            if (truthy(t)) { resolved_tenant = t; break; }
        }
    } else {
        resolved_tenant = (json_t *)tenant;
    }
    int blocked = initial_status && !strcmp(initial_status, "blocked");
    if (blocked) {
        json_t *a = json_array();
        json_append(a, json_string("blocked"));
        json_append(a, resolved_tenant ? json_copy(resolved_tenant) : json_null());
        free(prows);
        return a;
    }
    if (triage) {
        json_t *a = json_array();
        json_append(a, json_string("triage"));
        json_append(a, resolved_tenant ? json_copy(resolved_tenant) : json_null());
        free(prows);
        return a;
    }
    int all_done = 1;
    for (size_t i = 0; i < np; i++) {
        const char *st = jstr(json_obj_get(prows[i], "status"));
        if (!st || strcmp(st, "done") != 0) { all_done = 0; break; }
    }
    json_t *a = json_array();
    json_append(a, json_string(all_done ? "ready" : "todo"));
    json_append(a, resolved_tenant ? json_copy(resolved_tenant) : json_null());
    free(prows);
    return a;
}

static int blank_title(const char *s) {
    if (!s) return 1;
    while (*s && isspace((unsigned char)*s)) s++;
    return *s == '\0';
}

static void py_scalar(const json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); break;
        case JSON_STRING: snprintf(buf, n, "%s", v->str_val ? v->str_val : ""); break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, n, "%lld", (long long)v->num_val);
            else
                snprintf(buf, n, "%g", v->num_val);
            break;
        default: if (n) buf[0] = '\0'; break;
    }
}

static int int_value(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type != JSON_NUMBER) return 0;
    if (v->num_val != (double)(long long)v->num_val) return 0;
    *out = (long long)v->num_val;
    return 1;
}

json_t *w13_kanbandbg_30158b_validate_graph(const struct json_t *children) {
    size_t n = (children && children->type == JSON_ARRAY) ? children->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *child = json_get(children, i);
        if (!child || child->type != JSON_OBJECT)
            return err_obj("child[%zu] is not a dict", i);
        if (blank_title(jstr(json_obj_get(child, "title"))))
            return err_obj("child[%zu].title is required", i);
        json_t *par = json_obj_get(child, "parents");
        if (par && par->type == JSON_NULL) par = NULL;
        if (par && par->type == JSON_BOOL && !par->bool_val) par = NULL;
        if (par && par->type == JSON_NUMBER && par->num_val == 0.0) par = NULL;
        if (par && par->type == JSON_STRING && (!par->str_val || !par->str_val[0])) par = NULL;
        if (par && par->type == JSON_ARRAY && par->c.count == 0) par = NULL;
        if (par && par->type != JSON_ARRAY)
            return err_obj("child[%zu].parents must be a list", i);
        size_t pn = par ? par->c.count : 0;
        for (size_t j = 0; j < pn; j++) {
            json_t *pv = json_get(par, j);
            long long idx;
            char ps[128];
            py_scalar(pv, ps, sizeof(ps));
            if (!int_value(pv, &idx))
                return err_obj("child[%zu].parents[%s] is not a valid index into children", i, ps);
            if (idx < 0 || idx >= (long long)n)
                return err_obj("child[%zu].parents[%s] is not a valid index into children", i, ps);
            if (idx == (long long)i)
                return err_obj("child[%zu] cannot list itself as a parent", i);
        }
    }
    long long *indeg = (long long *)calloc(n ? n : 1, sizeof(long long));
    long long **adj = (long long **)calloc(n ? n : 1, sizeof(long long *));
    long long *acount = (long long *)calloc(n ? n : 1, sizeof(long long));
    long long *queue = (long long *)calloc(n ? n : 1, sizeof(long long));
    if (!indeg || !adj || !acount || !queue) {
        free(indeg); free(adj); free(acount); free(queue);
        return err_obj("out of memory");
    }
    for (size_t i = 0; i < n; i++) {
        adj[i] = (long long *)calloc(n ? n : 1, sizeof(long long));
        if (!adj[i]) { free(indeg); free(adj); free(acount); free(queue); return err_obj("out of memory"); }
    }
    for (size_t i = 0; i < n; i++) {
        json_t *par = json_obj_get(json_get(children, i), "parents");
        if (!par || par->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < par->c.count; j++) {
            long long p = (long long)json_get(par, j)->num_val;
            if (json_get(par, j)->type == JSON_BOOL) p = json_get(par, j)->bool_val ? 1 : 0;
            adj[p][acount[p]++] = (long long)i;
            indeg[i]++;
        }
    }
    size_t qh = 0, qt = 0;
    for (size_t i = 0; i < n; i++) if (indeg[i] == 0) queue[qt++] = (long long)i;
    long long seen = 0;
    while (qh < qt) {
        long long cur = queue[--qt];
        seen++;
        for (long long k = 0; k < acount[cur]; k++) {
            long long nb = adj[cur][k];
            if (--indeg[nb] == 0) queue[qt++] = nb;
        }
    }
    for (size_t i = 0; i < n; i++) free(adj[i]);
    free(indeg); free(adj); free(acount); free(queue);
    if (seen != (long long)n) return err_obj("cyclic dependency detected in decomposed children list");
    json_t *o = json_object();
    json_set(o, "ok", json_bool(1));
    return o;
}
