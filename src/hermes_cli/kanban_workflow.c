/*
 * kanban_workflow.c — C port of upstream Hermes hermes_cli/kanban_workflow.py.
 *
 * PoP: w13_kanbanwork_7d88c0_keys @ hermes_cli/kanban_workflow.py:Workflow.keys
 * PoP: w13_kanbanwork_7d88c0_column @ hermes_cli/kanban_workflow.py:Workflow.column
 * PoP: w13_kanbanwork_7d88c0_can_move @ hermes_cli/kanban_workflow.py:Workflow.can_move
 * PoP: w13_kanbanwork_7d88c0_to_dict @ hermes_cli/kanban_workflow.py:Workflow.to_dict
 * PoP: w13_kanbanwork_7d88c0_validate @ hermes_cli/kanban_workflow.py:Workflow.validate
 *
 * Pure Workflow data model; a caller-supplied workflow exercises the error paths.
 */
#include "noc_w13_kanbanwork_7d88c0.h"
#include "hermes_json.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARCHIVED "archived"

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static void add_col(json_t *cols, const char *key, const char *label,
                    const char *icon, int drag) {
    json_t *c = json_object();
    json_set(c, "key", json_string(key));
    json_set(c, "label", json_string(label));
    json_set(c, "icon", json_string(icon));
    json_set(c, "drag_target", json_bool(drag));
    json_append(cols, c);
}

static void add_man(json_t *man, const char *src, ...) {
    json_t *a = json_array();
    va_list ap;
    va_start(ap, src);
    const char *dst;
    while ((dst = va_arg(ap, const char *)) != NULL) json_append(a, json_string(dst));
    va_end(ap);
    json_set(man, src, a);
}

/* Mirrors DEFAULT_WORKFLOW from the live module (icons are UTF-8 glyphs). */
static json_t *default_workflow(void) {
    json_t *w = json_object();
    json_t *cols = json_array();
    add_col(cols, "triage", "Triage", "\xE2\x97\x87", 1);      /* diamond */
    add_col(cols, "todo", "Todo", "\xE2\x97\xBB", 1);          /* square */
    add_col(cols, "scheduled", "Scheduled", "\xE2\x8F\xB1", 0);/* clock */
    add_col(cols, "ready", "Ready", "\xE2\x96\xB6", 1);        /* play */
    add_col(cols, "running", "Running", "\xE2\x97\x8F", 0);    /* circle */
    add_col(cols, "blocked", "Blocked", "\xE2\x8A\x98", 1);    /* circled slash */
    add_col(cols, "review", "Review", "\xE2\x97\x8E", 0);      /* bullseye */
    add_col(cols, "done", "Done", "\xE2\x9C\x93", 1);          /* check */
    json_t *man = json_object();
    add_man(man, "triage", "todo", "ready", NULL);
    add_man(man, "todo", "triage", "scheduled", "ready", NULL);
    add_man(man, "scheduled", "triage", "todo", "ready", NULL);
    add_man(man, "ready", "triage", "todo", "scheduled", "blocked", "review", "done", NULL);
    add_man(man, "running", "triage", "todo", "scheduled", "ready", "blocked", "review", "done", NULL);
    add_man(man, "blocked", "triage", "todo", "scheduled", "ready", "done", NULL);
    add_man(man, "review", "triage", "todo", "ready", "done", NULL);
    add_man(man, "done", "triage", "todo", "ready", NULL);
    add_man(man, ARCHIVED, "triage", "todo", "ready", NULL);
    json_set(w, "columns", cols);
    json_set(w, "manual", man);
    return w;
}

/* Returns an owned workflow json_t. */
static json_t *workflow_of(const struct json_t *wf) {
    if (wf && wf->type == JSON_OBJECT) return json_copy(wf);
    return default_workflow();
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

/* Python repr() of a plain string, single-quoted. */
static char *py_repr(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(2 * n + 4);
    if (!out) return NULL;
    size_t j = 0;
    out[j++] = '\'';
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\'' || s[i] == '\\') out[j++] = '\\';
        out[j++] = s[i];
    }
    out[j++] = '\'';
    out[j] = '\0';
    return out;
}

json_t *w13_kanbanwork_7d88c0_keys(const struct json_t *workflow) {
    json_t *w = workflow_of(workflow);
    json_t *out = json_array();
    json_t *cols = json_obj_get(w, "columns");
    if (cols && cols->type == JSON_ARRAY)
        for (size_t i = 0; i < cols->c.count; i++) {
            const char *k = jstr(json_obj_get(json_get(cols, i), "key"));
            json_append(out, json_string(k ? k : ""));
        }
    json_free(w);
    return out;
}

json_t *w13_kanbanwork_7d88c0_column(const struct json_t *workflow, const char *key) {
    json_t *w = workflow_of(workflow);
    json_t *out = json_null();
    json_t *cols = json_obj_get(w, "columns");
    if (cols && cols->type == JSON_ARRAY && key)
        for (size_t i = 0; i < cols->c.count; i++) {
            json_t *c = json_get(cols, i);
            const char *k = jstr(json_obj_get(c, "key"));
            if (k && !strcmp(k, key)) { out = json_copy(c); break; }
        }
    json_free(w);
    return out;
}

static json_t *find_column(json_t *w, const char *key) {
    json_t *cols = json_obj_get(w, "columns");
    if (cols && cols->type == JSON_ARRAY && key)
        for (size_t i = 0; i < cols->c.count; i++) {
            json_t *c = json_get(cols, i);
            const char *k = jstr(json_obj_get(c, "key"));
            if (k && !strcmp(k, key)) return c;
        }
    return NULL;
}

static int manual_has(json_t *w, const char *src, const char *dst) {
    json_t *man = json_obj_get(w, "manual");
    json_t *dsts = (man && src) ? json_obj_get(man, src) : NULL;
    if (!dsts || dsts->type != JSON_ARRAY || !dst) return 0;
    for (size_t i = 0; i < dsts->c.count; i++) {
        const char *d = jstr(json_get(dsts, i));
        if (d && !strcmp(d, dst)) return 1;
    }
    return 0;
}

json_t *w13_kanbanwork_7d88c0_can_move(const struct json_t *workflow,
                                       const char *src, const char *dst) {
    json_t *w = workflow_of(workflow);
    if (!src) src = "";
    if (!dst) dst = "";
    if (strcmp(src, ARCHIVED) != 0 && !find_column(w, src)) {
        char *r = py_repr(src);
        json_t *e = err_obj("unknown kanban workflow column %s", r ? r : "''");
        free(r);
        json_free(w);
        return e;
    }
    json_t *out;
    if (!strcmp(src, dst)) {
        out = json_bool(0);
    } else if (!strcmp(dst, ARCHIVED)) {
        out = json_bool(1);
    } else {
        if (!find_column(w, dst)) {
            char *r = py_repr(dst);
            json_t *e = err_obj("unknown kanban workflow column %s", r ? r : "''");
            free(r);
            json_free(w);
            return e;
        }
        out = json_bool(manual_has(w, src, dst) ? 1 : 0);
    }
    json_free(w);
    return out;
}

static int cmp_strptr(const void *a, const void *b) {
    const char *const *x = (const char *const *)a;
    const char *const *y = (const char *const *)b;
    return strcmp(*x, *y);
}

json_t *w13_kanbanwork_7d88c0_to_dict(const struct json_t *workflow) {
    json_t *w = workflow_of(workflow);
    json_t *out = json_object();
    json_t *cols = json_obj_get(w, "columns");
    json_t *arr = json_array();
    if (cols && cols->type == JSON_ARRAY)
        for (size_t i = 0; i < cols->c.count; i++) {
            json_t *c = json_get(cols, i);
            json_t *o = json_object();
            json_set(o, "key", json_copy(json_obj_get(c, "key")));
            json_set(o, "label", json_copy(json_obj_get(c, "label")));
            json_set(o, "icon", json_copy(json_obj_get(c, "icon")));
            json_set(o, "drag_target", json_copy(json_obj_get(c, "drag_target")));
            json_append(arr, o);
        }
    json_set(out, "columns", arr);
    json_t *man = json_obj_get(w, "manual");
    json_t *mout = json_object();
    if (man && man->type == JSON_OBJECT)
        for (size_t i = 0; i < json_object_size(man); i++) {
            const char *k = json_object_get_key_at(man, i);
            json_t *v = json_object_get_at(man, i);
            json_t *dsts = json_array();
            if (v && v->type == JSON_ARRAY) {
                char **vals = (char **)calloc(v->c.count ? v->c.count : 1, sizeof(char *));
                size_t n = 0;
                if (vals) {
                    for (size_t j = 0; j < v->c.count; j++) {
                        const char *s = jstr(json_get(v, j));
                        if (s) vals[n++] = strdup(s);
                    }
                    if (n > 1) qsort(vals, n, sizeof(char *), cmp_strptr);
                    for (size_t j = 0; j < n; j++) { json_append(dsts, json_string(vals[j])); free(vals[j]); }
                    free(vals);
                }
            }
            json_set(mout, k ? k : "", dsts);
        }
    json_set(out, "manual", mout);
    json_set(out, "archived", json_string(ARCHIVED));
    json_free(w);
    return out;
}

static char *tuple_repr(json_t *cols) {
    size_t n = (cols && cols->type == JSON_ARRAY) ? cols->c.count : 0;
    size_t cap = 32 * (n + 1) + 4;
    char *buf = (char *)calloc(cap ? cap : 8, 1);
    if (!buf) return NULL;
    size_t at = 0;
    at += (size_t)snprintf(buf + at, cap - at, "(");
    for (size_t i = 0; i < n; i++) {
        const char *k = jstr(json_obj_get(json_get(cols, i), "key"));
        char *r = py_repr(k ? k : "");
        size_t rl = strlen(r ? r : "''");
        if (at + rl + 4 < cap) {
            if (i) { buf[at++] = ','; buf[at++] = ' '; }
            memcpy(buf + at, r ? r : "''", rl);
            at += rl;
        }
        free(r);
    }
    if (n == 1 && at + 1 < cap) buf[at++] = ',';
    if (at < cap) buf[at++] = ')';
    buf[at] = '\0';
    return buf;
}

json_t *w13_kanbanwork_7d88c0_validate(const struct json_t *workflow) {
    json_t *w = workflow_of(workflow);
    json_t *cols = json_obj_get(w, "columns");
    size_t n = (cols && cols->type == JSON_ARRAY) ? cols->c.count : 0;
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < i; j++) {
            const char *a = jstr(json_obj_get(json_get(cols, i), "key"));
            const char *b = jstr(json_obj_get(json_get(cols, j), "key"));
            if (a && b && !strcmp(a, b)) {
                char *r = tuple_repr(cols);
                json_t *e = err_obj("duplicate column keys: %s", r ? r : "()");
                free(r);
                json_free(w);
                return e;
            }
        }
    for (size_t i = 0; i < n; i++) {
        const char *k = jstr(json_obj_get(json_get(cols, i), "key"));
        if (k && !strcmp(k, ARCHIVED)) {
            json_free(w);
            return err_obj("'archived' is reserved and cannot be a column");
        }
    }
    json_t *man = json_obj_get(w, "manual");
    if (man && man->type == JSON_OBJECT)
        for (size_t i = 0; i < json_object_size(man); i++) {
            const char *src = json_object_get_key_at(man, i);
            int src_ok = 0;
            if (src && !strcmp(src, ARCHIVED)) src_ok = 1;
            else if (src && find_column(w, src)) src_ok = 1;
            json_t *dsts = json_object_get_at(man, i);
            int dsts_ok = 1;
            if (dsts && dsts->type == JSON_ARRAY)
                for (size_t j = 0; j < dsts->c.count; j++) {
                    const char *d = jstr(json_get(dsts, j));
                    if (!d || !find_column(w, d)) { dsts_ok = 0; break; }
                }
            if (!src_ok || !dsts_ok) {
                char *r = py_repr(src ? src : "");
                json_t *e = err_obj("manual moves from %s reference unknown columns", r ? r : "''");
                free(r);
                json_free(w);
                return e;
            }
        }
    json_free(w);
    json_t *o = json_object();
    json_set(o, "ok", json_bool(1));
    return o;
}
