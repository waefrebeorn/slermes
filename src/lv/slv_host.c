/*
 * slv_host.c -- the interpreter's public driver: state lifecycle, global
 * registration, run/call entry points and the builtin host functions.
 * See slv.h.
 */
#include "slv_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void slv_set_err(slv *L, const char *fmt, ...) {
    if (!L) return;
    /* First error wins. A guard (nesting limit) returning nil makes every
     * enclosing frame see a nil callee; without this, the cascading
     * "attempt to call a nil value" would overwrite the real cause. */
    if (L->has_err) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(L->err, sizeof L->err, fmt, ap);
    va_end(ap);
    L->has_err = 1;
}

slv *slv_alloc(void) {
    slv *L = (slv *)calloc(1, sizeof(*L));
    if (!L) return NULL;
    L->nbuckets = 64;
    L->buckets = (istr **)slv_arena_alloc(L, L->nbuckets * sizeof(*L->buckets));
    L->globals = slv_table_new(L, 0, 8);
    L->scap = 64;
    L->stack = (slv_value *)slv_arena_alloc(L, L->scap * sizeof(*L->stack));
    return L;
}

slv *slv_new(void) {
    slv *L = slv_alloc();
    if (L) slv_register_builtins(L);
    return L;
}

void slv_free(slv *L) {
    if (!L) return;
    anode *n = L->root, *nx;
    while (n) { nx = n->next; free(n); n = nx; }
    free(L);
}

void slv_register_global(slv *L, const char *name, slv_cfn fn, void *ud) {
    slv_value v; v.t = SLV_CFUNCTION; v.as.cf.f = fn; v.as.cf.ud = ud;
    slv_table_set(L, L->globals, slv_str(slv_intern(L, name, strlen(name))), v);
}

slv_value slv_get_global(slv *L, const char *name) {
    return slv_table_get(L, L->globals, slv_str(slv_intern(L, name, strlen(name))));
}

void slv_set_global(slv *L, const char *name, slv_value v) {
    slv_table_set(L, L->globals, slv_str(slv_intern(L, name, strlen(name))), v);
}

const char *slv_run(slv *L, const char *src, const char *chunkname) {
    (void)chunkname;
    L->has_err = 0; L->err[0] = 0;

    /* The AST pool is arena-backed, NOT on the C stack. slv_call() may be
     * used on a function defined by an earlier slv_run() (that is the
     * engine-loop contract), so closures must keep their body/params alive
     * after run() returns. One pool per state, allocated lazily and never
     * reset: nodes only accumulate for the life of the state. */
    if (!L->pool) {
        L->pool = SLV_ALLOC(pool, L);
        if (!L->pool) {
            slv_set_err(L, "out of memory");
            return L->err;
        }
    }

    ex *root = NULL;
    if (slv_parse_driver(L, src, L->pool, &root))
        return L->err[0] ? L->err : "parse error";
    slv_exec_root(L, root);
    return L->has_err ? L->err : NULL;
}

slv_value slv_call(slv *L, slv_value fn, slv_value *args, int nargs) {
    if (fn.t == SLV_FUNCTION) return slv_call_lua(L, fn.as.fn, args, nargs);
    if (fn.t == SLV_CFUNCTION) return fn.as.cf.f(L, args, nargs, fn.as.cf.ud);
    return slv_nil();
}

/* ---- builtin host functions (a tiny stdio/tonumber-ish floor) ---- */
static slv_value cfn_print(slv *L, slv_value *args, int n, void *ud) {
    (void)L; (void)ud;
    printf("%s\n", n>0 ? slv_tostring(args[0]) : "");
    return slv_nil();
}

static slv_value cfn_tonumber(slv *L, slv_value *args, int n, void *ud) {
    (void)L; (void)ud;
    if (n>0 && args[0].t == SLV_NUM) return slv_number(args[0].as.n);
    return slv_number(0);
}

static slv_value cfn_type(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    return slv_str(slv_intern(L,
                                  slv_type_name(n > 0 ? args[0].t : SLV_NIL),
                                  strlen(slv_type_name(n > 0 ? args[0].t : SLV_NIL))));
}

void slv_register_builtins(slv *L) {
    slv_register_global(L, "print", cfn_print, NULL);
    slv_register_global(L, "tonumber", cfn_tonumber, NULL);
    slv_register_global(L, "type", cfn_type, NULL);
}
