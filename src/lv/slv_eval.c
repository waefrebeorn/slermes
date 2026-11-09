/*
 * slv_eval.c -- the tree-walking evaluator: scopes, calls, operators with
 * metamethod dispatch, multiple returns, varargs, and error isolation.
 * See slv_internal.h.
 */
#include "slv_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- errors */

void slv_set_err(slv *L, const char *fmt, ...) {
    if (!L || L->has_err) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(L->err, sizeof L->err, fmt, ap);
    va_end(ap);
    L->has_err = 1;
}

/* Raise. If a pcall catch point is active, unwind to it; otherwise just mark
 * the state and let the eval loop stop at the next check. */
void slv_error(slv *L, const char *fmt, ...) {
    if (!L->has_err) {
        va_list ap; va_start(ap, fmt);
        vsnprintf(L->err, sizeof L->err, fmt, ap);
        va_end(ap);
        L->has_err = 1;
    }
    if (L->njmp > 0) longjmp(L->jmp[L->njmp - 1], 1);
}

jmp_buf *slv_push_catch(slv *L) {
    jmp_buf *n = (jmp_buf *)realloc(L->jmp, (size_t)(L->njmp + 1) * sizeof(jmp_buf));
    if (!n) return NULL;
    L->jmp = n;
    return &L->jmp[L->njmp++];
}
void slv_pop_catch(slv *L) { if (L->njmp > 0) L->njmp--; }

/* --------------------------------------------------------------- scopes */

static void env_set(slv *L, env *e, const char *name, slv_value v) {
    if (!e) return;
    slv_table_set(L, e->t, slv_str(name), v);
}

static slv_value env_get(slv *L, env *e, const char *name) {
    for (env *c = e; c; c = c->parent) {
        slv_value r = slv_table_get(L, c->t, slv_str(name));
        if (r.t != SLV_NIL) return r;
    }
    return slv_table_get(L, L->globals, slv_str(name));
}

static void env_assign(slv *L, env *e, const char *name, slv_value v) {
    for (env *c = e; c; c = c->parent) {
        slv_value r = slv_table_get(L, c->t, slv_str(name));
        if (r.t != SLV_NIL) { env_set(L, c, name, v); return; }
    }
    slv_table_set(L, L->globals, slv_str(name), v);
}

static env *new_env(slv *L, env *parent) {
    env *e = SLV_ALLOC(env, L);
    if (!e) return NULL;
    e->t = slv_table_new(L, 0, 8);
    e->parent = parent;
    return e;
}

/* ---------------------------------------------------------- value stack */

static void stk_push(slv *L, slv_value v) {
    if (L->sp >= L->scap) {
        int nc = L->scap ? L->scap * 2 : 64;
        slv_value *n = (slv_value *)slv_arena_alloc(L, (size_t)nc * sizeof(*n));
        memcpy(n, L->stk, (size_t)L->sp * sizeof(*n));
        L->stk = n; L->scap = nc;
    }
    L->stk[L->sp++] = v;
}

/* Build a SLV_MULTI value from an array (arena-allocated). */
slv_value slv_multi_value(slv *L, slv_value *v, int n) {
    if (n <= 0) return slv_nil();
    struct slv_multi *m = (struct slv_multi *)
        slv_arena_alloc(L, sizeof(struct slv_multi) + (size_t)n * sizeof(slv_value));
    m->n = n;
    if (v) memcpy(m->v, v, (size_t)n * sizeof(slv_value));
    slv_value r; r.t = SLV_MULTI; r.as.multi = m;
    return r;
}

/* ------------------------------------------------------------- operators */

static int64_t to_int(slv *L, slv_value v, int *ok) {
    if (v.t == SLV_INT) { if (ok) *ok = 1; return v.as.i; }
    if (v.t == SLV_NUM) {
        if (v.as.n >= -9.2233720368547758e18 && v.as.n < 9.2233720368547758e18) {
            if (ok) *ok = 1; return (int64_t)v.as.n;
        }
    }
    if (ok) *ok = 0;
    return 0;
}

static const char *arith_event(int op) {
    switch (op) {
    case '+': return "__add";
    case '-': return "__sub";
    case '*': return "__mul";
    case '/': return "__div";
    case T_IDIV: return "__idiv";
    case '%': return "__mod";
    case '^': return "__pow";
    case '&': return "__band";
    case '|': return "__bor";
    case '~': return "__bxor";
    case T_SHL: return "__shl";
    case T_SHR: return "__shr";
    }
    return NULL;
}

static slv_value call_meta(slv *L, slv_value f, slv_value a, slv_value b) {
    slv_value args[2]; args[0] = a; args[1] = b;
    return slv_call(L, f, args, 2);
}

slv_value slv_arith(slv *L, int op, slv_value a, slv_value b) {
    /* metamethods first, when neither operand is a plain number */
    if (!(a.t == SLV_INT || a.t == SLV_NUM) || !(b.t == SLV_INT || b.t == SLV_NUM)) {
        const char *ev = arith_event(op);
        if (ev) {
            slv_table *ma = slv_getmetatable(L, a);
            slv_table *mb = slv_getmetatable(L, b);
            slv_value f = ma ? slv_table_get(L, ma, slv_str(ev)) : slv_nil();
            if (f.t == SLV_NIL && mb) f = slv_table_get(L, mb, slv_str(ev));
            if (f.t == SLV_FUNCTION || f.t == SLV_CFUNCTION) return call_meta(L, f, a, b);
        }
        slv_error(L, "attempt to perform arithmetic on a %s value (line %d)",
                    slv_type_name((a.t == SLV_INT || a.t == SLV_NUM) ? b.t : a.t),
                    L->curline);
        return slv_nil();
    }

    int ai = (a.t == SLV_INT), bi = (b.t == SLV_INT);

    /* bitwise operations are integer-only */
    switch (op) {
    case '&': case '|': case '~': case T_SHL: case T_SHR: {
        int ok1, ok2;
        int64_t x = to_int(L, a, &ok1), y = to_int(L, b, &ok2);
        if (!ok1 || !ok2) {
            slv_error(L, "number has no integer representation (line %d)", L->curline);
            return slv_nil();
        }
        int64_t r = 0;
        switch (op) {
        case '&': r = x & y; break;
        case '|': r = x | y; break;
        case '~': r = x ^ y; break;
        case T_SHL: r = (y >= 0 && y < 64) ? (int64_t)((uint64_t)x << y) : (y >= 64 ? 0 : (int64_t)((uint64_t)x >> -y)); break;
        case T_SHR: r = (y >= 0 && y < 64) ? (int64_t)((uint64_t)x >> y) : (y >= 64 ? 0 : (int64_t)((uint64_t)x << -y)); break;
        }
        return slv_integer(r);
    }
    }

    if (ai && bi) {
        int64_t x = a.as.i, y = b.as.i, r = 0;
        switch (op) {
        case '+': r = (int64_t)((uint64_t)x + (uint64_t)y); break;
        case '-': r = (int64_t)((uint64_t)x - (uint64_t)y); break;
        case '*': r = (int64_t)((uint64_t)x * (uint64_t)y); break;
        case T_IDIV:
            if (y == 0) { slv_error(L, "attempt to divide by zero (line %d)", L->curline); return slv_nil(); }
            r = x / y;
            if ((x % y != 0) && ((x < 0) != (y < 0))) r--;
            return slv_integer(r);
        case '%':
            if (y == 0) { slv_error(L, "attempt to perform 'n%%0' (line %d)", L->curline); return slv_nil(); }
            r = x % y;
            if (r != 0 && ((r < 0) != (y < 0))) r += y;
            return slv_integer(r);
        case '^': return slv_number(pow((double)x, (double)y));
        }
        return slv_integer(r);
    }

    double x = (ai) ? (double)a.as.i : a.as.n;
    double y = (bi) ? (double)b.as.i : b.as.n;
    switch (op) {
    case '+': return slv_number(x + y);
    case '-': return slv_number(x - y);
    case '*': return slv_number(x * y);
    case '/': return slv_number(x / y);
    case '^': return slv_number(pow(x, y));
    case T_IDIV: {
        double r = floor(x / y);
        return slv_number(r);
    }
    case '%': {
        double r = x - floor(x / y) * y;
        return slv_number(r);
    }
    }
    return slv_nil();
}

slv_value slv_compare(slv *L, int op, slv_value a, slv_value b) {
    /* equality is defined across numeric types */
    if (op == T_EQ || op == T_NE) {
        int eq;
        if ((a.t == SLV_INT || a.t == SLV_NUM) && (b.t == SLV_INT || b.t == SLV_NUM)) {
            double x = (a.t == SLV_INT) ? (double)a.as.i : a.as.n;
            double y = (b.t == SLV_INT) ? (double)b.as.i : b.as.n;
            eq = (x == y);
        } else eq = slv_equal(L, a, b);
        return slv_bool(op == T_EQ ? eq : !eq);
    }

    /* ordering */
    if (a.t == SLV_STR && b.t == SLV_STR) {
        int c = strcmp(a.as.s, b.as.s);
        switch (op) {
        case '<': return slv_bool(c < 0);
        case '>': return slv_bool(c > 0);
        case T_LE: return slv_bool(c <= 0);
        case T_GE: return slv_bool(c >= 0);
        }
    }
    if ((a.t == SLV_INT || a.t == SLV_NUM) && (b.t == SLV_INT || b.t == SLV_NUM)) {
        double x = (a.t == SLV_INT) ? (double)a.as.i : a.as.n;
        double y = (b.t == SLV_INT) ? (double)b.as.i : b.as.n;
        switch (op) {
        case '<': return slv_bool(x < y);
        case '>': return slv_bool(x > y);
        case T_LE: return slv_bool(x <= y);
        case T_GE: return slv_bool(x >= y);
        }
    }
    /* metamethods __lt / __le. `a > b` is `b < a`, `a >= b` is `b <= a`. */
    const char *ev = (op == '<' || op == '>') ? "__lt" : "__le";
    slv_value x = a, y = b;
    if (op == '>' || op == T_GE) { x = b; y = a; }
    slv_table *mx = slv_getmetatable(L, x);
    slv_table *my = slv_getmetatable(L, y);
    slv_value f = mx ? slv_table_get(L, mx, slv_str(ev)) : slv_nil();
    if (f.t == SLV_NIL && my) f = slv_table_get(L, my, slv_str(ev));
    if (f.t == SLV_FUNCTION || f.t == SLV_CFUNCTION) {
        slv_value r = call_meta(L, f, x, y);
        return slv_bool(slv_truthy(r));
    }
    slv_error(L, "attempt to compare %s with %s (line %d)",
                slv_type_name(a.t), slv_type_name(b.t), L->curline);
    return slv_nil();
}

int slv_len(slv *L, slv_value v, size_t *out) {
    if (v.t == SLV_STR) { *out = strlen(v.as.s); return 1; }
    if (v.t == SLV_TABLE) {
        slv_table *m = slv_getmetatable(L, v);
        slv_value f = m ? slv_table_get(L, m, slv_str("__len")) : slv_nil();
        if (f.t == SLV_FUNCTION || f.t == SLV_CFUNCTION) {
            slv_value r = slv_call(L, f, &v, 1);
            *out = (size_t)slv_tonumber(r, NULL);
            return 1;
        }
        *out = slv_table_len(v.as.tab);
        return 1;
    }
    slv_error(L, "attempt to get length of a %s value (line %d)", slv_type_name(v.t), L->curline);
    return 0;
}

slv_value slv_concat(slv *L, slv_value a, slv_value b) {
    int an = (a.t == SLV_STR || a.t == SLV_INT || a.t == SLV_NUM);
    int bn = (b.t == SLV_STR || b.t == SLV_INT || b.t == SLV_NUM);
    if (an && bn) {
        const char *sa = slv_tostring_intern(L, a);
        const char *sb = slv_tostring_intern(L, b);
        size_t la = strlen(sa), lb = strlen(sb);
        char *buf = (char *)malloc(la + lb + 1);
        if (!buf) { slv_error(L, "out of memory"); return slv_nil(); }
        memcpy(buf, sa, la); memcpy(buf + la, sb, lb); buf[la + lb] = 0;
        const char *r = slv_intern(L, buf, la + lb);
        free(buf);
        return slv_str(r);
    }
    slv_table *ma = slv_getmetatable(L, a);
    slv_table *mb = slv_getmetatable(L, b);
    slv_value f = ma ? slv_table_get(L, ma, slv_str("__concat")) : slv_nil();
    if (f.t == SLV_NIL && mb) f = slv_table_get(L, mb, slv_str("__concat"));
    if (f.t == SLV_FUNCTION || f.t == SLV_CFUNCTION) return call_meta(L, f, a, b);
    slv_error(L, "attempt to concatenate a %s value (line %d)",
                slv_type_name(an ? b.t : a.t), L->curline);
    return slv_nil();
}

/* ------------------------------------------------------------- indexing */

slv_value slv_index(slv *L, slv_value obj, slv_value key) {
    for (int loop = 0; loop < 100; loop++) {
        if (obj.t == SLV_TABLE) {
            slv_value v = slv_table_get(L, obj.as.tab, key);
            if (v.t != SLV_NIL) return v;
            slv_table *m = obj.as.tab->meta;
            if (!m) return slv_nil();
            slv_value h = slv_table_get(L, m, slv_str("__index"));
            if (h.t == SLV_NIL) return slv_nil();
            if (h.t == SLV_FUNCTION || h.t == SLV_CFUNCTION) {
                slv_value args[2]; args[0] = obj; args[1] = key;
                return slv_call(L, h, args, 2);
            }
            obj = h;                     /* __index is a table: repeat */
            continue;
        }
        if (obj.t == SLV_STR) {
            /* string methods live on the string metatable */
            slv_table *m = L->string_meta;
            if (m) {
                slv_value v = slv_table_get(L, m, key);
                if (v.t != SLV_NIL) return v;
            }
            slv_error(L, "attempt to index a string value (line %d)", L->curline);
            return slv_nil();
        }
        slv_table *m = slv_getmetatable(L, obj);
        if (m) {
            slv_value h = slv_table_get(L, m, slv_str("__index"));
            if (h.t == SLV_FUNCTION || h.t == SLV_CFUNCTION) {
                slv_value args[2]; args[0] = obj; args[1] = key;
                return slv_call(L, h, args, 2);
            }
        }
        slv_error(L, "attempt to index a %s value (line %d)", slv_type_name(obj.t), L->curline);
        return slv_nil();
    }
    slv_error(L, "'__index' chain too long; possible loop (line %d)", L->curline);
    return slv_nil();
}

void slv_setindex(slv *L, slv_value obj, slv_value key, slv_value v) {
    for (int loop = 0; loop < 100; loop++) {
        if (obj.t == SLV_TABLE) {
            slv_table *m = obj.as.tab->meta;
            if (m) {
                slv_value h = slv_table_get(L, m, slv_str("__newindex"));
                if (h.t == SLV_FUNCTION || h.t == SLV_CFUNCTION) {
                    slv_value args[3]; args[0] = obj; args[1] = key; args[2] = v;
                    slv_call(L, h, args, 3);
                    return;
                }
                if (h.t == SLV_TABLE) { obj = h; continue; }
            }
            slv_table_set(L, obj.as.tab, key, v);
            return;
        }
        slv_error(L, "attempt to index a %s value (line %d)", slv_type_name(obj.t), L->curline);
        return;
    }
}

/* ---------------------------------------------------------------- eval */

static slv_value eval_impl(slv *L, ex *e, env *en);
static slv_value exec(slv *L, ex *e, env *en, int *was_return);
static slv_value exec(slv *L, ex *e, env *en, int *was_return) {
    return slv_exec(L, e, en, was_return);
}

static slv_value eval(slv *L, ex *e, env *en) {
    if (!e) return slv_nil();
    if (L->eval_depth >= SLV_MAX_EVAL_DEPTH) {
        slv_error(L, "expression nested too deeply (limit %d)", SLV_MAX_EVAL_DEPTH);
        return slv_nil();
    }
    int save = L->curline;
    L->curline = e->line;
    L->eval_depth++;
    slv_value r = eval_impl(L, e, en);
    L->eval_depth--;
    L->curline = save;
    return r;
}

/* call a function value. Results are copied into an arena buffer and left to
 * the caller; returns the result count. */
int slv_call_value(slv *L, slv_value fn, slv_value *args, int nargs, slv_value **out) {
    *out = NULL;
    if (fn.t == SLV_CFUNCTION) {
        slv_value r = fn.as.cf.f(L, args, nargs, fn.as.cf.ud);
        if (L->has_err) return 0;
        if (r.t == SLV_MULTI && r.as.multi) {
            int n = r.as.multi->n;
            if (n <= 0) return 0;
            slv_value *res = SLV_ALLOCN(slv_value, n, L);
            memcpy(res, r.as.multi->v, (size_t)n * sizeof(*res));
            *out = res; return n;
        }
        slv_value *res = SLV_ALLOCN(slv_value, 1, L);
        res[0] = r;
        *out = res; return 1;
    }
    if (fn.t == SLV_FUNCTION) {
        int base = L->sp;
        int n = slv_call_lua_results(L, fn.as.fn, args, nargs);
        if (L->has_err) { L->sp = base; return 0; }
        if (n <= 0) { L->sp = base; return 0; }
        slv_value *res = SLV_ALLOCN(slv_value, n, L);
        memcpy(res, L->stk + base, (size_t)n * sizeof(*res));
        L->sp = base;
        *out = res; return n;
    }
    /* __call metamethod */
    slv_table *m = slv_getmetatable(L, fn);
    if (m) {
        slv_value h = slv_table_get(L, m, slv_str("__call"));
        if (h.t == SLV_FUNCTION || h.t == SLV_CFUNCTION) {
            slv_value *a2 = (slv_value *)malloc((size_t)(nargs + 1) * sizeof(*a2));
            if (!a2) { slv_error(L, "out of memory"); return 0; }
            a2[0] = fn;
            if (nargs) memcpy(a2 + 1, args, (size_t)nargs * sizeof(*a2));
            int r = slv_call_value(L, h, a2, nargs + 1, out);
            free(a2);
            return r;
        }
    }
    slv_error(L, "attempt to call a %s value (line %d)", slv_type_name(fn.t), L->curline);
    return 0;
}

slv_value slv_eval_multi(slv *L, ex *e, env *en, slv_value **out, int *nout) {
    *out = NULL; *nout = 0;
    if (!e) return slv_nil();

    if (e->t == N_CALL || e->t == N_METHOD) {
        slv_value callee, *args;
        int na;
        if (e->t == N_METHOD) {
            slv_value self = eval(L, e->e1->e1, en);
            if (L->has_err) return slv_nil();
            slv_value key = (e->e1->e2 && e->e1->e2->t == N_STR)
                                  ? slv_str(e->e1->e2->str) : slv_nil();
            callee = slv_index(L, self, key);
            if (L->has_err) return slv_nil();
            na = e->nlist + 1;
            args = (slv_value *)malloc((size_t)(na > 0 ? na : 1) * sizeof(*args));
            if (!args) { slv_error(L, "out of memory"); return slv_nil(); }
            args[0] = self;
            for (int i = 0; i < e->nlist; i++) {
                if (i == e->nlist - 1) {
                    slv_value *o2 = NULL; int n2 = 0;
                    slv_eval_multi(L, e->list[i], en, &o2, &n2);
                    args[i + 1] = (n2 > 0) ? o2[0] : slv_nil();
                } else args[i + 1] = eval(L, e->list[i], en);
            }
        } else {
            callee = eval(L, e->e1, en);
            if (L->has_err) return slv_nil();
            na = e->nlist;
            args = (slv_value *)malloc((size_t)(na > 0 ? na : 1) * sizeof(*args));
            if (!args) { slv_error(L, "out of memory"); return slv_nil(); }
            for (int i = 0; i < na; i++) {
                if (i == na - 1 && na > 0) {
                    slv_value *o2 = NULL; int n2 = 0;
                    slv_eval_multi(L, e->list[i], en, &o2, &n2);
                    args[i] = (n2 > 0) ? o2[0] : slv_nil();
                } else args[i] = eval(L, e->list[i], en);
            }
        }
        if (L->has_err) { free(args); return slv_nil(); }
        int nr = slv_call_value(L, callee, args, na, out);
        free(args);
        if (L->has_err) return slv_nil();
        *nout = nr;
        return (nr > 0) ? (*out)[0] : slv_nil();
    }

    if (e->t == N_VARARG) {
        *out = L->va; *nout = L->nva;
        return (L->nva > 0) ? L->va[0] : slv_nil();
    }

    slv_value v = eval(L, e, en);
    *nout = 1;
    stk_push(L, v);
    *out = L->stk + L->sp - 1;
    return v;
}

static slv_value eval_impl(slv *L, ex *e, env *en) {
    if (!e) return slv_nil();
    switch (e->t) {
    case N_NIL: return slv_nil();
    case N_TRUE: return slv_bool(1);
    case N_FALSE: return slv_bool(0);
    case N_INT: return slv_integer(e->i);
    case N_NUM: return slv_number(e->num);
    case N_STR: return slv_str(e->str);
    case N_NAME: return env_get(L, en, e->name);
    case N_PAREN: {
        /* parenthesised: exactly one value */
        if (e->e1 && (e->e1->t == N_CALL || e->e1->t == N_METHOD || e->e1->t == N_VARARG)) {
            slv_value *out = NULL; int n = 0;
            slv_eval_multi(L, e->e1, en, &out, &n);
            return (n > 0) ? out[0] : slv_nil();
        }
        return eval(L, e->e1, en);
    }
    case N_INDEX: {
        slv_value obj = eval(L, e->e1, en);
        slv_value key = eval(L, e->e2, en);
        if (L->has_err) return slv_nil();
        return slv_index(L, obj, key);
    }
    case N_AND: {
        slv_value a = eval(L, e->e1, en);
        if (!slv_truthy(a)) return a;
        return eval(L, e->e2, en);
    }
    case N_OR: {
        slv_value a = eval(L, e->e1, en);
        if (slv_truthy(a)) return a;
        return eval(L, e->e2, en);
    }
    case N_UNARY: {
        slv_value v = eval(L, e->e1, en);
        if (L->has_err) return slv_nil();
        switch (e->op) {
        case T_NOT: return slv_bool(!slv_truthy(v));
        case '-':
            if (v.t == SLV_INT) return slv_integer((int64_t)(0 - (uint64_t)v.as.i));
            if (v.t == SLV_NUM) return slv_number(-v.as.n);
            /* __unm */
            {
                slv_table *m = slv_getmetatable(L, v);
                if (m) {
                    slv_value f = slv_table_get(L, m, slv_str("__unm"));
                    if (f.t == SLV_FUNCTION || f.t == SLV_CFUNCTION) return call_meta(L, f, v, v);
                }
            }
            slv_error(L, "attempt to perform arithmetic on a %s value (line %d)", slv_type_name(v.t), L->curline);
            return slv_nil();
        case '~':
            if (v.t == SLV_INT) return slv_integer(~v.as.i);
            if (v.t == SLV_NUM) {
                int ok; int64_t x = to_int(L, v, &ok);
                if (!ok) { slv_error(L, "number has no integer representation (line %d)", L->curline); return slv_nil(); }
                return slv_integer(~x);
            }
            slv_error(L, "attempt to perform bitwise operation on a %s value (line %d)", slv_type_name(v.t), L->curline);
            return slv_nil();
        case '#': {
            size_t n;
            if (!slv_len(L, v, &n)) return slv_nil();
            return slv_integer((int64_t)n);
        }
        }
        return slv_nil();
    }
    case N_BINOP: {
        slv_value a = eval(L, e->e1, en);
        if (L->has_err) return slv_nil();
        slv_value b = eval(L, e->e2, en);
        if (L->has_err) return slv_nil();
        if (e->op == T_CONCAT) return slv_concat(L, a, b);
        if (e->op == T_EQ || e->op == T_NE || e->op == '<' || e->op == '>' ||
            e->op == T_LE || e->op == T_GE)
            return slv_compare(L, e->op, a, b);
        return slv_arith(L, e->op, a, b);
    }
    case N_TABLE: {
        slv_table *t = slv_table_new(L, e->nlist, e->nlist);
        for (int i = 0; i < e->nlist; i++) {
            if (e->keys[i]) {
                slv_value k = eval(L, e->keys[i], en);
                slv_value v = eval(L, e->list[i], en);
                if (L->has_err) return slv_nil();
                slv_table_set(L, t, k, v);
            } else if (i == e->nlist - 1 && (e->list[i]->t == N_CALL || e->list[i]->t == N_METHOD || e->list[i]->t == N_VARARG)) {
                slv_value *out = NULL; int n = 0;
                slv_eval_multi(L, e->list[i], en, &out, &n);
                for (int j = 0; j < n; j++) {
                    slv_value k; k.t = SLV_INT; k.as.i = (int64_t)(i + j + 1);
                    slv_table_set(L, t, k, out[j]);
                }
            } else {
                slv_value v = eval(L, e->list[i], en);
                if (L->has_err) return slv_nil();
                slv_value k; k.t = SLV_INT; k.as.i = (int64_t)(i + 1);
                slv_table_set(L, t, k, v);
            }
        }
        slv_value r; r.t = SLV_TABLE; r.as.tab = t;
        return r;
    }
    case N_FUNC: {
        slv_func *fn = SLV_ALLOC(slv_func, L);
        if (!fn) { slv_error(L, "out of memory"); return slv_nil(); }
        fn->body = e->e1; fn->name = e->name; fn->defining = en;
        fn->nparams = e->nlist;
        fn->params = e->names;
        fn->varargs = NULL; fn->nvarargs = 0;
        slv_value r; r.t = SLV_FUNCTION; r.as.fn = fn;
        return r;
    }
    default:
        /* statements and calls reach here only when used as expressions */
        return exec(L, e, en, &(int){0});
    }
}

/* ------------------------------------------------------- function calls */

/* Run a Lua function body. Results are PUSHED onto the value stack (the
 * caller reads L->stk[base..sp) and rewinds); returns the result count. */
int slv_call_lua_results(slv *L, slv_func *fn, slv_value *args, int nargs) {
    if (L->depth >= SLV_MAX_CALL) {
        slv_error(L, "call nesting too deep (limit %d)", SLV_MAX_CALL);
        return 0;
    }
    L->depth++;

    env *e = new_env(L, fn->defining);
    if (!e) { L->depth--; slv_error(L, "out of memory"); return 0; }
    for (int i = 0; i < fn->nparams; i++)
        env_set(L, e, fn->params[i], (i < nargs) ? args[i] : slv_nil());

    /* varargs: everything past the named parameters */
    int nva = (nargs > fn->nparams) ? nargs - fn->nparams : 0;
    slv_value *va = NULL;
    if (nva > 0) {
        va = SLV_ALLOCN(slv_value, nva, L);
        memcpy(va, args + fn->nparams, (size_t)nva * sizeof(*va));
    }
    slv_value *save_va = L->va; int save_nva = L->nva;
    L->va = va; L->nva = nva;

    int base = L->sp;
    if (fn->body && fn->body->t == N_BLOCK) {
        int wr = 0;
        exec(L, fn->body, e, &wr);
    }
    int n = L->sp - base;

    L->va = save_va; L->nva = save_nva;
    L->depth--;
    return n;
}

/* single-value contract used by the public slv_call */
slv_value slv_call_lua(slv *L, slv_func *fn, slv_value *args, int nargs) {
    int base = L->sp;
    int n = slv_call_lua_results(L, fn, args, nargs);
    slv_value r = slv_nil();
    if (n > 0) { r = L->stk[base]; L->sp = base; }
    return r;
}

/* ------------------------------------------------------------- executor */

static int block_has_label(ex *b, const char *label) {
    if (!b) return 0;
    for (int i = 0; i < b->nlist; i++)
        if (b->list[i] && b->list[i]->t == N_LABEL && !strcmp(b->list[i]->label, label)) return 1;
    return 0;
}

slv_value slv_exec(slv *L, ex *e, env *en, int *was_return) {
    *was_return = 0;
    if (!e) return slv_nil();
    int save = L->curline;
    L->curline = e->line;

    switch (e->t) {
    case N_BLOCK: {
        env *be = new_env(L, en);           /* every block gets its own scope */
        int n = e->nlist;
        for (int i = 0; i < n; i++) {
            ex *st = e->list[i];
            if (st && st->t == N_GOTO) {
                if (block_has_label(e, st->label)) {
                    /* find the label and resume after it */
                    int j = i;
                    for (; j < n; j++) if (e->list[j]->t == N_LABEL && !strcmp(e->list[j]->label, st->label)) break;
                    if (j < n) { i = j; continue; }
                }
                L->goto_label = st->label; L->has_goto = 1;
                L->curline = save;
                return slv_nil();
            }
            int wr = 0;
            slv_value r = exec(L, st, be, &wr);
            if (wr) { *was_return = 1; L->curline = save; return r; }
            if (L->has_err) { L->curline = save; return slv_nil(); }
            if (L->has_goto) { L->curline = save; return slv_nil(); }
        }
        L->curline = save;
        return slv_nil();
    }
    case N_LOCAL: {
        int nvals = (int)(intptr_t)e->e4;
        ex **vals = (ex **)e->keys;
        int nn = e->nlist;
        if (nvals == 0) {
            for (int i = 0; i < nn; i++) env_set(L, en, e->names[i], slv_nil());
        } else if (nvals == 1 && nn >= 1) {
            slv_value v;
            if (vals[0]->t == N_CALL || vals[0]->t == N_METHOD || vals[0]->t == N_VARARG) {
                slv_value *out = NULL; int no = 0;
                slv_eval_multi(L, vals[0], en, &out, &no);
                v = (no > 0) ? out[0] : slv_nil();
            } else v = eval(L, vals[0], en);
            env_set(L, en, e->names[0], v);
            for (int i = 1; i < nn; i++) env_set(L, en, e->names[i], slv_nil());
        } else {
            for (int i = 0; i < nvals - 1 && i < nn; i++) env_set(L, en, e->names[i], eval(L, vals[i], en));
            slv_value *out = NULL; int no = 0;
            if (nvals > 0) slv_eval_multi(L, vals[nvals - 1], en, &out, &no);
            int start = nvals - 1;
            for (int i = 0; i < no && start + i < nn; i++) env_set(L, en, e->names[start + i], out[i]);
            for (int i = start + no; i < nn; i++) env_set(L, en, e->names[i], slv_nil());
        }
        L->curline = save;
        return slv_nil();
    }
    case N_ASSIGN: {
        int nt = e->nlist, nv = (int)(intptr_t)e->e4;
        ex **vals = (ex **)e->keys;
        /* evaluate the right-hand side first */
        slv_value rhs[32];
        int nr = 0;
        for (int i = 0; i < nv - 1 && i < 32; i++) rhs[nr++] = eval(L, vals[i], en);
        slv_value *out = NULL; int no = 0;
        if (nv > 0) slv_eval_multi(L, vals[nv - 1], en, &out, &no);
        for (int i = 0; i < no && nr < 32; i++) rhs[nr++] = out[i];

        for (int i = 0; i < nt; i++) {
            ex *t = e->list[i];
            slv_value v = (i < nr) ? rhs[i] : slv_nil();
            if (t->t == N_NAME) env_assign(L, en, t->name, v);
            else if (t->t == N_INDEX) {
                slv_value obj = eval(L, t->e1, en);
                slv_value key = eval(L, t->e2, en);
                if (!L->has_err) slv_setindex(L, obj, key, v);
            }
            if (L->has_err) break;
        }
        L->curline = save;
        return slv_nil();
    }
    case N_CALL: case N_METHOD: {
        slv_value *out = NULL; int no = 0;
        slv_eval_multi(L, e, en, &out, &no);
        L->curline = save;
        return slv_nil();
    }
    case N_FUNCSTAT: {
        slv_value f = eval(L, e->e1, en);
        env_assign(L, en, e->name, f);
        L->curline = save;
        return slv_nil();
    }
    case N_LOCALFUNC: {
        slv_value f = eval(L, e->e1, en);
        env_set(L, en, e->name, f);
        L->curline = save;
        return slv_nil();
    }
    case N_IF: {
        slv_value c = eval(L, e->e1, en);
        if (L->has_err) { L->curline = save; return slv_nil(); }
        if (slv_truthy(c)) { slv_value r = exec(L, e->e2, en, was_return); L->curline = save; return r; }
        for (int i = 0; i < e->nlist; i++) {
            slv_value cc = eval(L, e->list[i * 2], en);
            if (L->has_err) { L->curline = save; return slv_nil(); }
            if (slv_truthy(cc)) { slv_value r = exec(L, e->list[i * 2 + 1], en, was_return); L->curline = save; return r; }
        }
        if (e->e3) { slv_value r = exec(L, e->e3, en, was_return); L->curline = save; return r; }
        L->curline = save;
        return slv_nil();
    }
    case N_WHILE: {
        for (;;) {
            slv_value c = eval(L, e->e1, en);
            if (L->has_err) break;
            if (!slv_truthy(c)) break;
            int wr = 0;
            slv_value r = exec(L, e->e2, en, &wr);
            if (wr) { *was_return = 1; L->curline = save; return r; }
            if (L->has_err) break;
            if (L->has_goto) break;
        }
        L->curline = save;
        return slv_nil();
    }
    case N_REPEAT: {
        for (;;) {
            int wr = 0;
            slv_value r = exec(L, e->e2, en, &wr);
            if (wr) { *was_return = 1; L->curline = save; return r; }
            if (L->has_err || L->has_goto) break;
            slv_value c = eval(L, e->e1, en);
            if (L->has_err) break;
            if (slv_truthy(c)) break;
        }
        L->curline = save;
        return slv_nil();
    }
    case N_FORNUM: {
        slv_value v1 = eval(L, e->e1, en);
        slv_value v2 = eval(L, e->e2, en);
        slv_value v3 = e->e3 ? eval(L, e->e3, en) : slv_integer(1);
        if (L->has_err) { L->curline = save; return slv_nil(); }
        int ok1, ok2, ok3;
        double start = slv_tonumber(v1, &ok1);
        double stop  = slv_tonumber(v2, &ok2);
        double step  = slv_tonumber(v3, &ok3);
        if (!ok1 || !ok2 || !ok3) {
            slv_error(L, "'for' initial value must be a number (line %d)", e->line);
            L->curline = save; return slv_nil();
        }
        if (step == 0) { slv_error(L, "'for' step is zero (line %d)", e->line); L->curline = save; return slv_nil(); }
        for (double i = start; (step > 0) ? (i <= stop) : (i >= stop); i += step) {
            env *fe = new_env(L, en);
            if (!fe) break;
            env_set(L, fe, e->names[0], slv_number(i));
            int wr = 0;
            slv_value r = exec(L, e->e4, fe, &wr);
            if (wr) { *was_return = 1; L->curline = save; return r; }
            if (L->has_err || L->has_goto) break;
        }
        L->curline = save;
        return slv_nil();
    }
    case N_FORIN: {
        /* evaluate the iterator triple */
        int nv = e->nlist;
        slv_value f, s, ctrl;
        slv_value *out = NULL; int no = 0;
        slv_eval_multi(L, e->list[nv - 1], en, &out, &no);
        f = (no > 0) ? out[0] : slv_nil();
        s = (no > 1) ? out[1] : slv_nil();
        ctrl = (no > 2) ? out[2] : slv_nil();
        while (!L->has_err) {
            slv_value args[2]; args[0] = s; args[1] = ctrl;
            slv_value *res = NULL; int nr = slv_call_value(L, f, args, 2, &res);
            if (L->has_err) break;
            if (nr == 0 || res[0].t == SLV_NIL) break;
            ctrl = res[0];
            env *fe = new_env(L, en);
            if (!fe) break;
            for (int i = 0; i < e->nlist && i < nr; i++) {
                env_set(L, fe, e->names[i], res[i]);
            }
            int wr = 0;
            slv_value r = exec(L, e->e4, fe, &wr);
            if (wr) { *was_return = 1; L->curline = save; return r; }
            if (L->has_err || L->has_goto) break;
        }
        L->curline = save;
        return slv_nil();
    }
    case N_DO: {
        env *de = new_env(L, en);
        slv_value r = exec(L, e->e1, de ? de : en, was_return);
        L->curline = save;
        return r;
    }
    case N_RETURN: {
        int n = e->nlist;
        ex **vals = e->list;
        if (n == 0) { *was_return = 1; L->curline = save; return slv_nil(); }
        /* push results onto the value stack */
        for (int i = 0; i < n - 1; i++) stk_push(L, eval(L, vals[i], en));
        slv_value *out = NULL; int no = 0;
        slv_eval_multi(L, vals[n - 1], en, &out, &no);
        for (int i = 0; i < no; i++) stk_push(L, out[i]);
        if (no == 0) stk_push(L, slv_nil());
        *was_return = 1;
        L->curline = save;
        return slv_nil();
    }
    case N_BREAK:
        /* break is signalled by a sentinel: reuse has_goto with a NULL label */
        L->goto_label = NULL; L->has_goto = 1;
        L->curline = save;
        return slv_nil();
    case N_GOTO:
        L->goto_label = e->label; L->has_goto = 1;
        L->curline = save;
        return slv_nil();
    case N_LABEL:
        /* labels are no-ops at run time */
        L->curline = save;
        return slv_nil();
    default: {
        slv_value r = eval(L, e, en);
        L->curline = save;
        return r;
    }
    }
}

slv_value slv_exec_root(slv *L, ex *root) {
    env *top = new_env(L, NULL);
    if (!top) { slv_error(L, "out of memory"); return slv_nil(); }
    int wr = 0;
    exec(L, root, top, &wr);
    return slv_nil();
}
