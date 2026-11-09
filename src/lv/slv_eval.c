/*
 * slv_eval.c -- the tree-walking evaluator: scopes, calls and statements.
 * See slv_internal.h.
 */
#include "slv_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Scopes are linked frames holding a name->value table. Simple and correct
 * for the subset; not as fast as registers -- fine at this milestone. */
static void env_set(slv *L, env *e, const char *name, slv_value v) {
    if (!e) return;
    slv_table_set(L, e->t, slv_str(slv_intern(L, name, strlen(name))), v);
}

static slv_value env_get(slv *L, env *e, const char *name) {
    for (env *c = e; c; c = c->parent) {
        slv_value r = slv_table_get(L, c->t, slv_str(slv_intern(L, name, strlen(name))));
        if (r.t != SLV_NIL) return r;
    }
    /* fall through to globals */
    return slv_table_get(L, L->globals, slv_str(slv_intern(L, name, strlen(name))));
}

static void env_assign(slv *L, env *e, const char *name, slv_value v) {
    /* find nearest frame with the name; else global */
    for (env *c = e; c; c = c->parent) {
        slv_value r = slv_table_get(L, c->t, slv_str(slv_intern(L, name, strlen(name))));
        if (r.t != SLV_NIL) { env_set(L, c, name, v); return; }
    }
    slv_table_set(L, L->globals, slv_str(slv_intern(L, name, strlen(name))), v);
}

static slv_value eval_impl(slv *L, ex *e, env *en);
static slv_value exec(slv *L, ex *e, env *en, int *was_return);

/* Expression-evaluation nesting guard. A left-deep AST (e.g. 5000 postfix
 * calls like `(1)(1)(1)...`) recurses once per node and would otherwise
 * exhaust the C stack before any parser limit is hit. */
static slv_value eval(slv *L, ex *e, env *en) {
    if (L->eval_depth >= SLV_MAX_EVAL_DEPTH) {
        slv_set_err(L, "expression nested too deeply (limit %d)", SLV_MAX_EVAL_DEPTH);
        return slv_nil();
    }
    L->eval_depth++;
    slv_value r = eval_impl(L, e, en);
    L->eval_depth--;
    return r;
}

/* call a lua function value */
slv_value slv_call_lua(slv *L, slv_func *fn, slv_value *args, int nargs) {
    /* recursion guard: unbounded script self-recursion would eat the C stack */
    if (L->depth >= SLV_MAX_CALL) {
        slv_set_err(L, "call nesting too deep (limit %d)", SLV_MAX_CALL);
        return slv_nil();
    }
    L->depth++;

    /* new env: params bound, then body; use exec() so `return` propagates. */
    env *e = SLV_ALLOC(env, L);
    e->t = slv_table_new(L, 0, 8);
    e->parent = fn->defining;
    for (int i = 0; i < fn->nparams && i < nargs; i++) env_set(L, e, fn->params[i], args[i]);
    for (int i = nargs; i < fn->nparams; i++) env_set(L, e, fn->params[i], slv_nil());
    slv_value ret = slv_nil();
    if (fn->body && fn->body->t == N_BLOCK) {
        int wr = 0;
        for (int i = 0; i < fn->body->nlist; i++) {
            ret = exec(L, fn->body->list[i], e, &wr);
            if (wr) { L->depth--; return ret; }      /* `return expr` fired */
            if (L->has_err) { L->depth--; return ret; }
        }
    }
    L->depth--;
    return slv_nil();
}

/* run the value of an expression */
static slv_value eval_impl(slv *L, ex *e, env *en) {
    if (!e) return slv_nil();
    switch (e->t) {
    case N_EXPR: return eval(L, e->e1, en);   /* expression statement */
    case N_NUM: return slv_number(e->num);
    case N_STR: return slv_str(e->str);
    case N_BOOL:return slv_bool(e->boolean);
    case N_NIL: return slv_nil();
    case N_NAME: return env_get(L, en, e->name);
    case N_INDEX: {
        slv_value obj = eval(L, e->e1, en);
        if (obj.t != SLV_TABLE) {
            slv_set_err(L, "attempt to index a %s value", slv_type_name(obj.t));
            return slv_nil();
        }
        slv_value key = eval(L, e->e2, en);
        return slv_table_get(L, obj.as.tab, key);
    }
    case N_UNARY: {
        slv_value v = eval(L, e->e1, en);
        if (e->num == T_NOT) return slv_bool(!slv_truthy(v));
        if (e->num == '-') { double d = slv_tonumber(v, NULL); return slv_number(-d); }
        return slv_nil();
    }
    case N_BINOP: {
        slv_value a = eval(L, e->e1, en), b = eval(L, e->e2, en);
        int op = (int)e->num;
        if (op == T_AND) return slv_truthy(a) ? b : a;
        if (op == T_OR)  return slv_truthy(a) ? a : b;
        /* Strings compare lexicographically, as Lua does. Previously every
         * comparison coerced non-numbers to 0, so 'a' < 'b' was false. */
        if ((op == '<' || op == '>' || op == T_LE || op == T_GE) &&
            a.t == SLV_STR && b.t == SLV_STR) {
            int c = strcmp(a.as.s, b.as.s);
            switch (op) {
            case '<':  return slv_bool(c < 0);
            case '>':  return slv_bool(c > 0);
            case T_LE: return slv_bool(c <= 0);
            case T_GE: return slv_bool(c >= 0);
            }
        }
        double x = a.t==SLV_NUM?a.as.n:0, y = b.t==SLV_NUM?b.as.n:0;
        switch (op) {
        case '+': return slv_number(x+y);
        case '-': return slv_number(x-y);
        case '*': return slv_number(x*y);
        /* Lua yields inf/nan on a zero divisor, not nil */
        case '/': return slv_number(x/y);
        case '%': return y!=0 ? slv_number(fmod(x,y)) : slv_number(nan(""));
        case '<': return slv_bool(x<y);
        case '>': return slv_bool(x>y);
        case T_LE: return slv_bool(x<=y);
        case T_GE: return slv_bool(x>=y);
        case T_EQ: return slv_bool(slv_equal(a,b));
        case T_NE: return slv_bool(!slv_equal(a,b));
        case T_CONCAT: {
            /* slv_tostring() returns a shared static buffer for numbers, so
             * copy a numeric operand BEFORE asking for the other side --
             * otherwise `1 .. 2` produced "22". */
            char abuf[64], bbuf[64];
            const char *sa = slv_tostring(a);
            if (a.t != SLV_STR) { snprintf(abuf, sizeof abuf, "%s", sa); sa = abuf; }
            const char *sb = slv_tostring(b);
            if (b.t != SLV_STR) { snprintf(bbuf, sizeof bbuf, "%s", sb); sb = bbuf; }
            size_t la=strlen(sa), lb=strlen(sb);
            char *buf = (char *)malloc(la+lb+1);
            if (!buf) return slv_nil();
            memcpy(buf,sa,la); memcpy(buf+la,sb,lb); buf[la+lb]=0;
            const char *in = slv_intern(L, buf, la+lb);
            free(buf);
            return slv_str(in);
        }
        }
        return slv_nil();
    }
    case N_TABLE: {
        slv_table *t = slv_table_new(L, 0, e->nlist);
        for (int i = 0; i < e->nlist; i++) {
            slv_value val = eval(L, e->list[i], en);
            if (e->keys[i]) { slv_value k = eval(L, e->keys[i], en); slv_table_set(L, t, k, val); }
            else { slv_table_set(L, t, slv_number(i+1), val); }
        }
        slv_value r; r.t = SLV_TABLE; r.as.tab = t; return r;
    }
    case N_FUNC: {
        slv_func *fn = SLV_ALLOC(slv_func, L);
        fn->body = e->e1; fn->name = e->name; fn->defining = en;
        fn->nparams = e->nlist;
        const char **params = SLV_ALLOCN(const char*, e->nlist ? e->nlist : 1, L);
        for (int i = 0; i < e->nlist; i++) params[i] = e->list[i] ? e->list[i]->name : NULL;
        fn->params = params;
        slv_value r; r.t = SLV_FUNCTION; r.as.fn = fn; return r;
    }
    case N_CALL: {
        if (e->is_method) {
            /* receiver passed as first arg; callee is receiver[key] */
            slv_value self = eval(L, e->receiver, en);
            slv_value callee = eval(L, e->e1, en);
            int na = e->nlist + 1;
            slv_value *args = (slv_value *)malloc((na ? na : 1)*sizeof(*args));
            args[0] = self;
            for (int i = 0; i < e->nlist; i++) args[1+i] = eval(L, e->list[i], en);
            if (callee.t == SLV_FUNCTION) { slv_value r = slv_call_lua(L, callee.as.fn, args, na); free(args); return r; }
            if (callee.t == SLV_CFUNCTION) { slv_value r = callee.as.cf.f(L, args, na, callee.as.cf.ud); free(args); return r; }
            slv_set_err(L, "attempt to call a %s value", slv_type_name(callee.t));
            free(args); return slv_nil();
        }
        slv_value callee = eval(L, e->e1, en);
        int na = e->nlist;
        slv_value *args = (slv_value *)malloc((na?na:1)*sizeof(*args));
        for (int i = 0; i < e->nlist; i++) args[i] = eval(L, e->list[i], en);
        if (callee.t == SLV_FUNCTION) { slv_value r = slv_call_lua(L, callee.as.fn, args, na); free(args); return r; }
        if (callee.t == SLV_CFUNCTION) { slv_value r = callee.as.cf.f(L, args, na, callee.as.cf.ud); free(args); return r; }
        slv_set_err(L, "attempt to call a %s value", slv_type_name(callee.t));
        free(args); return slv_nil();
    }
    case N_LOCAL:
    case N_ASSIGN:
    case N_IF:
    case N_WHILE:
    case N_FOR:
    case N_RETURN:
    case N_BLOCK:
        /* statements handled by exec() */
        return slv_nil();
    }
    return slv_nil();
}

/* exec a statement; returns a "return value" if e is a return (else NIL w/ flag) */
static slv_value exec(slv *L, ex *e, env *en, int *was_return) {
    *was_return = 0;
    if (!e) return slv_nil();
    switch (e->t) {
    case N_LOCAL: {
        /* values are in e->e3 (the vals array), count = e->num */
        int n = e->nlist;
        for (int i = 0; i < n; i++) {
            slv_value v = slv_nil();
            if (e->e3 && i < (int)e->num) v = eval(L, ((ex**)e->e3)[i], en);
            const char *nm = e->list[i] ? e->list[i]->name : NULL;
            if (nm) env_set(L, en, nm, v);
        }
        return slv_nil();
    }
    case N_ASSIGN: {
        if (e->e1 && e->e1->t == N_NAME) {
            slv_value v = eval(L, e->e2, en);
            env_assign(L, en, e->e1->name, v);
        } else if (e->e1 && e->e1->t == N_INDEX) {
            slv_value obj = eval(L, e->e1->e1, en);
            slv_value idx = eval(L, e->e1->e2, en);
            slv_value v = eval(L, e->e2, en);
            if (obj.t == SLV_TABLE) slv_table_set(L, obj.as.tab, idx, v);
        }
        return slv_nil();
    }
    case N_EXPR:   /* expression statement: evaluate, discard the result */
        eval(L, e->e1, en);
        return slv_nil();
    case N_CALL:   /* a call node reaching exec directly */
        eval(L, e, en);
        return slv_nil();
    case N_FUNC: {  /* function name(...) body -- define global + assign */
        slv_value f = eval(L, e, en);
        env_assign(L, en, e->name, f);
        return slv_nil();
    }
    case N_IF: {
        slv_value c = eval(L, e->e1, en);
        if (slv_truthy(c)) return exec(L, e->e2, en, was_return);
        if (e->e3) return exec(L, e->e3, en, was_return);
        return slv_nil();
    }
    case N_WHILE: {
        while (slv_truthy(eval(L, e->e1, en))) {
            slv_value r = exec(L, e->e2, en, was_return);
            if (*was_return) return r;
            if (L->has_err) return slv_nil();
        }
        return slv_nil();
    }
    case N_FOR: {
        double start = slv_tonumber(eval(L, e->e1, en), NULL);
        double stop  = slv_tonumber(eval(L, e->e2, en), NULL);
        for (double i = start; i <= stop; i++) {
            env_set(L, en, e->name, slv_number(i));
            slv_value r = exec(L, e->e3, en, was_return);
            if (*was_return) return r;
            if (L->has_err) return slv_nil();
        }
        return slv_nil();
    }
    case N_RETURN:
        *was_return = 1;
        return e->e1 ? eval(L, e->e1, en) : slv_nil();
    case N_BLOCK: {
        for (int i = 0; i < e->nlist; i++) {
            slv_value r = exec(L, e->list[i], en, was_return);
            if (*was_return) return r;
            if (L->has_err) return r;
        }
        return slv_nil();
    }
    default:
        /* expression as statement (N_* value) */
        return eval(L, e, en);
    }
}

slv_value slv_exec_root(slv *L, ex *root) {
    env *top = SLV_ALLOC(env, L);
    top->t = slv_table_new(L, 0, 8);
    top->parent = NULL;
    int wr = 0;
    (void)exec(L, root, top, &wr);
    if (L->has_err) return slv_nil();
    return slv_nil();
}
