/*
 * slv_host.c -- the interpreter's public driver: state lifecycle, the base
 * library, run/call entry points and pcall-based error isolation.
 * See slv.h.
 */
#include "slv_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- state */

const char *slv_version(void) { return "Lua 5.4"; }

slv *slv_alloc(void) {
    slv *L = (slv *)calloc(1, sizeof(*L));
    if (!L) return NULL;
    L->nbuckets = 64;
    L->buckets = (istr **)slv_arena_alloc(L, L->nbuckets * sizeof(*L->buckets));
    L->globals = slv_table_new(L, 0, 32);
    L->scap = 64;
    L->stk = (slv_value *)slv_arena_alloc(L, (size_t)L->scap * sizeof(*L->stk));
    L->rand_state[0] = 0x853c49e6748fea9bULL;
    L->rand_state[1] = 0xda3e39cb94b95bdbULL;
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
    free(L->jmp);
    free(L);
}

void slv_register_global(slv *L, const char *name, slv_cfn fn, void *ud) {
    slv_value v; v.t = SLV_CFUNCTION; v.as.cf.f = fn; v.as.cf.ud = ud;
    slv_table_set(L, L->globals, slv_str(name), v);
}

slv_value slv_get_global(slv *L, const char *name) {
    return slv_table_get(L, L->globals, slv_str(name));
}

void slv_set_global(slv *L, const char *name, slv_value v) {
    slv_table_set(L, L->globals, slv_str(name), v);
}

const char *slv_run(slv *L, const char *src, const char *chunkname) {
    L->has_err = 0; L->err[0] = 0;
    L->chunkname = chunkname ? chunkname : "?";
    L->curline = 1;

    if (!L->pool) {
        L->pool = SLV_ALLOC(pool, L);
        if (!L->pool) { slv_set_err(L, "out of memory"); return L->err; }
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

int slv_pcall(slv *L, slv_value fn, slv_value *args, int nargs,
                slv_value *out, int *nout) {
    jmp_buf *jb = slv_push_catch(L);
    if (!jb) { slv_set_err(L, "out of memory"); return 1; }
    if (setjmp(*jb) == 0) {
        int nr = slv_call_value(L, fn, args, nargs, &out);
        slv_pop_catch(L);
        *nout = nr;
        return 0;
    }
    slv_pop_catch(L);
    *nout = 0;
    /* stash the message where the caller can find it */
    slv_set_global(L, "__lv_last_error", slv_str(L->err));
    L->has_err = 0; L->err[0] = 0;
    return 1;
}

/* ---------------------------------------------------------- base library */

static slv_value cfn_print(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    for (int i = 0; i < n; i++) {
        if (i) printf("\t");
        slv_value v = args[i];
        /* __tostring */
        slv_table *m = slv_getmetatable(L, v);
        if (m) {
            slv_value f = slv_table_get(L, m, slv_str("__tostring"));
            if (f.t == SLV_FUNCTION || f.t == SLV_CFUNCTION) {
                v = slv_call(L, f, &args[i], 1);
            }
        }
        printf("%s", slv_tostring_intern(L, v));
    }
    printf("\n");
    return slv_nil();
}

static slv_value cfn_type(slv *L, slv_value *args, int n, void *ud) {
    (void)L; (void)ud;
    return slv_str(slv_type_name(n > 0 ? args[0].t : SLV_NIL));
}

static slv_value cfn_tostring(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    slv_value v = (n > 0) ? args[0] : slv_nil();
    slv_table *m = slv_getmetatable(L, v);
    if (m) {
        slv_value f = slv_table_get(L, m, slv_str("__tostring"));
        if (f.t == SLV_FUNCTION || f.t == SLV_CFUNCTION) v = slv_call(L, f, &args[0], 1);
    }
    return slv_str(slv_tostring_intern(L, v));
}

static int str2num(const char *s, double *out, int *isint) {
    char *end = NULL;
    /* Lua accepts leading/trailing spaces and a sign */
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        long long v = strtoll(s + 2, &end, 16);
        if (end == s + 2 || *end) return 0;
        *out = (double)v; if (isint) *isint = 1;
        return 1;
    }
    long long iv = strtoll(s, &end, 10);
    if (end != s && (*end == 0 || *end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
        if (*end == 0) { *out = (double)iv; if (isint) *isint = 1; return 1; }
    }
    double d = strtod(s, &end);
    if (end == s) return 0;
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
    if (*end != 0) return 0;
    *out = d; if (isint) *isint = 0;
    return 1;
}

static slv_value cfn_tonumber(slv *L, slv_value *args, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1 || args[0].t == SLV_NIL) return slv_nil();
    if (args[0].t == SLV_INT) return args[0];
    if (args[0].t == SLV_NUM) return args[0];
    if (args[0].t != SLV_STR) return slv_nil();
    /* optional base */
    if (n >= 2) {
        int b = (int)slv_tonumber(args[1], NULL);
        if (b < 2 || b > 36) { slv_error(L, "base out of range"); return slv_nil(); }
        char *end = NULL;
        long long v = strtoll(args[0].as.s, &end, b);
        if (end == args[0].as.s || *end) return slv_nil();
        return slv_integer(v);
    }
    double d; int isint;
    if (!str2num(args[0].as.s, &d, &isint)) return slv_nil();
    return isint ? slv_integer((int64_t)d) : slv_number(d);
}

static slv_value cfn_assert(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 1 || !slv_truthy(args[0])) {
        slv_value m = (n >= 2) ? args[1] : slv_str("assertion failed!");
        slv_error(L, "%s", slv_tostring_intern(L, m));
        return slv_nil();
    }
    return args[0];
}

static slv_value cfn_error(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    slv_value m = (n >= 1) ? args[0] : slv_nil();
    slv_error(L, "%s", slv_tostring_intern(L, m));
    return slv_nil();
}

static slv_value cfn_pcall(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 1) { slv_error(L, "bad argument #1 to 'pcall' (value expected)"); return slv_nil(); }
    jmp_buf *jb = slv_push_catch(L);
    if (!jb) { slv_set_err(L, "out of memory"); return slv_nil(); }
    if (setjmp(*jb) == 0) {
        slv_value *out = NULL;
        int nr = slv_call_value(L, args[0], args + 1, n - 1, &out);
        slv_pop_catch(L);
        if (L->has_err) { L->has_err = 0; L->err[0] = 0; }
        slv_value *all = SLV_ALLOCN(slv_value, nr + 1, L);
        all[0] = slv_bool(1);
        for (int i = 0; i < nr; i++) all[i + 1] = out[i];
        return slv_multi_value(L, all, nr + 1);
    }
    slv_pop_catch(L);
    slv_value msg = slv_str(L->err[0] ? L->err : "error");
    L->has_err = 0; L->err[0] = 0;
    slv_value all[2]; all[0] = slv_bool(0); all[1] = msg;
    return slv_multi_value(L, all, 2);
}

static slv_value cfn_xpcall(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 2) { slv_error(L, "bad argument #2 to 'xpcall' (value expected)"); return slv_nil(); }
    jmp_buf *jb = slv_push_catch(L);
    if (!jb) { slv_set_err(L, "out of memory"); return slv_nil(); }
    if (setjmp(*jb) == 0) {
        slv_value *out = NULL;
        int nr = slv_call_value(L, args[0], args + 2, n - 2, &out);
        slv_pop_catch(L);
        slv_value *all = SLV_ALLOCN(slv_value, nr + 1, L);
        all[0] = slv_bool(1);
        for (int i = 0; i < nr; i++) all[i + 1] = out[i];
        return slv_multi_value(L, all, nr + 1);
    }
    slv_pop_catch(L);
    slv_value msg = slv_str(L->err[0] ? L->err : "error");
    L->has_err = 0; L->err[0] = 0;
    slv_value hargs[1]; hargs[0] = msg;
    slv_value h = slv_call(L, args[1], hargs, 1);
    slv_value all[2]; all[0] = slv_bool(0); all[1] = h;
    return slv_multi_value(L, all, 2);
}

static slv_value cfn_select(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 1) { slv_error(L, "bad argument #1 to 'select' (number expected)"); return slv_nil(); }
    if (args[0].t == SLV_STR && args[0].as.s[0] == '#') {
        return slv_integer(n - 1);
    }
    int64_t i = slv_tointeger(args[0], NULL);
    if (i < 0) i = (int64_t)n + i;          /* negative index from the end */
    else if (i > (int64_t)n - 1) { slv_error(L, "bad argument #1 to 'select' (index out of range)"); return slv_nil(); }
    if (i < 1) { slv_error(L, "bad argument #1 to 'select' (index out of range)"); return slv_nil(); }
    int cnt = n - (int)i;
    if (cnt <= 0) return slv_nil();
    return slv_multi_value(L, args + i, cnt);
}

static slv_value cfn_next(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 1 || args[0].t != SLV_TABLE) { slv_error(L, "bad argument #1 to 'next' (table expected)"); return slv_nil(); }
    slv_value k = (n >= 2) ? args[1] : slv_nil();
    slv_value v;
    if (!slv_table_next(args[0].as.tab, &k, &v)) return slv_nil();
    slv_value all[2]; all[0] = k; all[1] = v;
    return slv_multi_value(L, all, 2);
}

/* the `pairs` iterator: next, t, nil */
static slv_value iter_next(slv *L, slv_value *args, int n, void *ud) {
    return cfn_next(L, args, n, ud);
}

static slv_value cfn_pairs(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 1) { slv_error(L, "bad argument #1 to 'pairs' (table expected)"); return slv_nil(); }
    slv_table *m = slv_getmetatable(L, args[0]);
    if (m) {
        slv_value f = slv_table_get(L, m, slv_str("__pairs"));
        if (f.t == SLV_FUNCTION || f.t == SLV_CFUNCTION) {
            slv_value *out = NULL;
            int nr = slv_call_value(L, f, args, 1, &out);
            return slv_multi_value(L, out, nr);
        }
    }
    slv_value all[3];
    all[0].t = SLV_CFUNCTION; all[0].as.cf.f = iter_next; all[0].as.cf.ud = NULL;
    all[1] = args[0];
    all[2] = slv_nil();
    return slv_multi_value(L, all, 3);
}

static slv_value iter_ipairs(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 2) return slv_nil();
    int64_t i = slv_tointeger(args[1], NULL) + 1;
    slv_value k; k.t = SLV_INT; k.as.i = i;
    slv_value v = slv_index(L, args[0], k);
    if (v.t == SLV_NIL) return slv_nil();
    slv_value all[2]; all[0] = k; all[1] = v;
    return slv_multi_value(L, all, 2);
}

static slv_value cfn_ipairs(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 1) { slv_error(L, "bad argument #1 to 'ipairs' (table expected)"); return slv_nil(); }
    slv_value all[3];
    all[0].t = SLV_CFUNCTION; all[0].as.cf.f = iter_ipairs; all[0].as.cf.ud = NULL;
    all[1] = args[0];
    all[2] = slv_integer(0);
    return slv_multi_value(L, all, 3);
}

static slv_value cfn_rawget(slv *L, slv_value *args, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 2 || args[0].t != SLV_TABLE) return slv_nil();
    return slv_table_get(L, args[0].as.tab, args[1]);
}

static slv_value cfn_rawset(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 3 || args[0].t != SLV_TABLE) return args[0];
    slv_table_set(L, args[0].as.tab, args[1], args[2]);
    return args[0];
}

static slv_value cfn_rawequal(slv *L, slv_value *args, int n, void *ud) {
    (void)L; (void)ud;
    return slv_bool(n >= 2 && slv_rawequal(args[0], args[1]));
}

static slv_value cfn_rawlen(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    size_t len = 0;
    if (n >= 1 && (args[0].t == SLV_TABLE || args[0].t == SLV_STR))
        slv_len(L, args[0], &len);
    return slv_integer((int64_t)len);
}

static slv_value cfn_setmetatable(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 1 || args[0].t != SLV_TABLE) { slv_error(L, "bad argument #1 to 'setmetatable' (table expected)"); return slv_nil(); }
    slv_table *m = (n >= 2 && args[1].t == SLV_TABLE) ? args[1].as.tab : NULL;
    slv_setmetatable(L, args[0].as.tab, m);
    return args[0];
}

static slv_value cfn_getmetatable(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 1) return slv_nil();
    slv_table *m = slv_getmetatable(L, args[0]);
    if (!m) return slv_nil();
    slv_value mt = slv_table_get(L, m, slv_str("__metatable"));
    if (mt.t != SLV_NIL) return mt;
    slv_value r; r.t = SLV_TABLE; r.as.tab = m;
    return r;
}

static slv_value cfn_load(slv *L, slv_value *args, int n, void *ud) {
    (void)ud;
    if (n < 1) return slv_nil();
    const char *src = NULL;
    if (args[0].t == SLV_STR) src = args[0].as.s;
    else {
        /* a function chunk: call it repeatedly and concatenate */
        slv_value f = args[0];
        char *buf = NULL; size_t len = 0, cap = 0;
        for (;;) {
            slv_value *out = NULL;
            int nr = slv_call_value(L, f, NULL, 0, &out);
            if (L->has_err) break;
            if (nr == 0 || out[0].t == SLV_NIL) break;
            if (out[0].t != SLV_STR) break;
            size_t l = strlen(out[0].as.s);
            if (len + l + 1 > cap) { cap = (len + l + 1) * 2; buf = (char *)realloc(buf, cap); }
            memcpy(buf + len, out[0].as.s, l); len += l;
            if (buf) buf[len] = 0;
        }
        src = buf ? buf : "";
        /* fall through: compile below */
        L->pool = L->pool;   /* no-op */
        slv_value *out = NULL;
        int nr = slv_call_value(L, slv_nil(), NULL, 0, &out);
        (void)nr;
        /* compile */
        if (!L->pool) L->pool = SLV_ALLOC(pool, L);
        ex *root = NULL;
        const char *err = slv_parse_driver(L, src, L->pool, &root);
        free(buf);
        if (err) {
            slv_value all[2]; all[0] = slv_nil(); all[1] = slv_str(err);
            L->has_err = 0; L->err[0] = 0;
            return slv_multi_value(L, all, 2);
        }
        slv_func *fn = SLV_ALLOC(slv_func, L);
        fn->body = root; fn->name = "=chunk"; fn->defining = NULL;
        fn->nparams = 0; fn->params = NULL;
        slv_value r; r.t = SLV_FUNCTION; r.as.fn = fn;
        return r;
    }
    if (!L->pool) L->pool = SLV_ALLOC(pool, L);
    ex *root = NULL;
    const char *err = slv_parse_driver(L, src, L->pool, &root);
    if (err) {
        slv_value all[2]; all[0] = slv_nil(); all[1] = slv_str(err);
        L->has_err = 0; L->err[0] = 0;
        return slv_multi_value(L, all, 2);
    }
    slv_func *fn = SLV_ALLOC(slv_func, L);
    fn->body = root; fn->name = "=chunk"; fn->defining = NULL;
    fn->nparams = 0; fn->params = NULL;
    slv_value r; r.t = SLV_FUNCTION; r.as.fn = fn;
    return r;
}

static slv_value cfn_collectgarbage(slv *L, slv_value *args, int n, void *ud) {
    (void)L; (void)args; (void)n; (void)ud;
    return slv_integer(0);
}

static slv_value cfn_unimplemented(slv *L, slv_value *args, int n, void *ud) {
    (void)args; (void)n;
    slv_error(L, "%s is not implemented in slerm-Lua", (const char *)ud);
    return slv_nil();
}

/* ------------------------------------------------------------ registration */

void slv_register_builtins(slv *L) {
    slv_register_global(L, "print", cfn_print, NULL);
    slv_register_global(L, "type", cfn_type, NULL);
    slv_register_global(L, "tostring", cfn_tostring, NULL);
    slv_register_global(L, "tonumber", cfn_tonumber, NULL);
    slv_register_global(L, "assert", cfn_assert, NULL);
    slv_register_global(L, "error", cfn_error, NULL);
    slv_register_global(L, "pcall", cfn_pcall, NULL);
    slv_register_global(L, "xpcall", cfn_xpcall, NULL);
    slv_register_global(L, "select", cfn_select, NULL);
    slv_register_global(L, "next", cfn_next, NULL);
    slv_register_global(L, "pairs", cfn_pairs, NULL);
    slv_register_global(L, "ipairs", cfn_ipairs, NULL);
    slv_register_global(L, "rawget", cfn_rawget, NULL);
    slv_register_global(L, "rawset", cfn_rawset, NULL);
    slv_register_global(L, "rawequal", cfn_rawequal, NULL);
    slv_register_global(L, "rawlen", cfn_rawlen, NULL);
    slv_register_global(L, "setmetatable", cfn_setmetatable, NULL);
    slv_register_global(L, "getmetatable", cfn_getmetatable, NULL);
    slv_register_global(L, "load", cfn_load, NULL);
    slv_register_global(L, "collectgarbage", cfn_collectgarbage, NULL);
    slv_register_global(L, "unpack", cfn_unimplemented, "unpack (use table.unpack)");

    /* _VERSION */
    slv_set_global(L, "_VERSION", slv_str("Lua 5.4"));

    /* _G: the globals table, self-referential */
    slv_value g; g.t = SLV_TABLE; g.as.tab = L->globals;
    slv_table_set(L, L->globals, slv_str("_G"), g);

    /* require / dofile / loadfile: stubs until a package system exists */
    slv_register_global(L, "require", cfn_unimplemented, "require");
    slv_register_global(L, "dofile", cfn_unimplemented, "dofile");
    slv_register_global(L, "loadfile", cfn_unimplemented, "loadfile");
}

void slv_openlibs(slv *L) {
    slv_register_builtins(L);
    slv_register_string(L);
    slv_register_table(L);
    slv_register_math(L);
    slv_register_os(L);
    slv_register_io(L);
}
