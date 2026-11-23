/*
 * session_worktree.c - C home for upstream Hermes hermes_cli/session_worktree.py.
 * PoP: w15_sessionwor_066f13_frozen_cwd @ hermes_cli/session_worktree.py:_frozen_cwd
 * PoP: w15_sessionwor_066f13_restore @ hermes_cli/session_worktree.py:restore
 *
 * Pure cwd/restore decisions: the receipt policy, session row, directory check and
 * recreate result are injected as JSON inputs, so no DB or filesystem is touched.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_sessionwor_066f13.h"

static char *dupstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str() for the scalar cwd values used here. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%g", v->num_val);
        return dupstr(b);
    }
    char *s = json_dumps(v, 0);
    return s ? s : dupstr("");
}

/* .strip() in place; NUL written at the new length. */
static void strip_in_place(char *s) {
    char *a = s, *b = s + strlen(s);
    while (a < b && is_ws((unsigned char)a[0])) a++;
    while (b > a && is_ws((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    memmove(s, a, n);
    s[n] = 0;
}

/* str(db.persistence.bindings.receipt(id).policy.get("cwd") or "").strip() */
struct json_t *w15_sessionwor_066f13_frozen_cwd(const struct json_t *policy) {
    if (!policy || policy->type != JSON_OBJECT) return json_string("");
    json_t *cwd = json_obj_get(policy, "cwd");
    if (!cwd || cwd->type == JSON_NULL) return json_string("");
    char *s = py_str(cwd);
    strip_in_place(s);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* restore(): frozen cwd else row cwd; empty or existing dir -> None; else recreate() */
struct json_t *w15_sessionwor_066f13_restore(const char *frozen_cwd, const struct json_t *record_cwd,
                                             int is_dir, const struct json_t *recreated) {
    char *cwd = NULL;
    if (frozen_cwd && frozen_cwd[0]) {
        cwd = dupstr(frozen_cwd);
    } else {
        cwd = py_str(record_cwd); /* (record.cwd or "").strip() */
        strip_in_place(cwd);
    }
    if (!cwd[0] || is_dir) {
        free(cwd);
        return json_null();
    }
    free(cwd);
    return recreated ? json_copy(recreated) : json_null();
}

