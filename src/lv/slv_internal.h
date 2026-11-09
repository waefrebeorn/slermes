/*
 * slv_internal.h -- private interface of the slerm-Lua interpreter.
 *
 * Included only by the slv*.c translation units. The public header keeps
 * slv / slv_value / slv_func / slv_table opaque; this is where the
 * state, the AST and the parser live, so no caller can reach into them.
 */
#ifndef SLV_INTERNAL_H
#define SLV_INTERNAL_H

#include "slv.h"

#include <stddef.h>
#include <stdio.h>

/* ---------------------------------------------------------------- arena */
/*
 * NON-intrusive arena: an 8-byte header precedes each block, so it works on
 * any struct. (An intrusive arena that writes `next` into the block's first
 * field silently corrupts interners and tables.)
 */
typedef struct anode { struct anode *next; } anode;

void *slv_arena_alloc(slv *L, size_t n);
#define SLV_ALLOC(T, L)     ((T *)slv_arena_alloc((L), sizeof(T)))
#define SLV_ALLOCN(T, n, L) ((T *)slv_arena_alloc((L), (n) * sizeof(T)))

/* ------------------------------------------------------------------ AST */

typedef enum {
    N_NUM, N_STR, N_BOOL, N_NIL, N_NAME, N_INDEX, N_BINOP, N_UNARY, N_TABLE,
    N_CALL, N_FUNC, N_LOCAL, N_ASSIGN, N_IF, N_WHILE, N_FOR, N_RETURN, N_BLOCK,
    N_EXPR,   /* expression statement: e1 is the expression, result discarded */
} ntype;

typedef struct ex {
    ntype t; int line;
    double num; const char *str; int boolean; const char *name;
    struct ex *e1, *e2, *e3;          /* operands / cond / body / else */
    struct ex **list; int nlist;      /* stmts / args / params / fields */
    struct ex **keys;                 /* table: {list=values, keys=key-expr} */
    int is_method; struct ex *receiver;
} ex;

#define SLV_MAX_EX 8192
/* Parser nesting limit (Lua's own limit is 200). Without it a few thousand
 * nested parens exhaust the C stack. */
#define SLV_MAX_DEPTH 200
/* Expression-evaluation nesting limit. Each Lua call level costs ~2 eval
 * levels (the call node in the return statement + the recursive call), so this
 * allows roughly 200 script call levels -- Lua's own limit. */
#define SLV_MAX_EVAL_DEPTH 400
/* Lua call nesting limit; guards against unbounded script recursion. */
#define SLV_MAX_CALL 200
typedef struct { ex n[SLV_MAX_EX]; int cnt; } pool;

/* ---------------------------------------------------------------- state */

struct slv {
    struct anode *root;
    struct istr **buckets; size_t nbuckets;
    slv_table *globals;
    slv_value *stack; int sp, scap;
    char err[512]; int has_err;
    int depth;          /* current Lua call nesting (recursion guard) */
    int eval_depth;     /* expression evaluation nesting (recursion guard) */
    pool *pool;         /* AST pool, arena-backed so closures outlive slv_run */
};

/* --------------------------------------------------------- string pool */

typedef struct istr { char *s; size_t len; unsigned hash; struct istr *next; } istr;

/* --------------------------------------------------------------- tables */

typedef struct slv_kv { slv_value k, v; int used; } slv_kv;
struct slv_table { slv_kv *e; size_t cap, count; };

/* --------------------------------------------------------------- parser */

typedef enum {
    T_EOF=0, T_NUM=256, T_STR, T_NAME,
    T_IF,T_THEN,T_ELSE,T_ELSEIF,T_END,T_WHILE,T_DO,T_FOR,T_FUNCTION,
    T_LOCAL,T_RETURN,T_IN,T_TRUE,T_FALSE,T_NIL,T_NOT,T_AND,T_OR,
    T_NE,T_LE,T_GE,T_CONCAT,T_EQ,   /* multi-char/compound */
} tktype;

typedef struct { int type; double num; const char *str; int line; } tok;

#define SLV_TKCHAR(c) ((int)(unsigned char)(c))   /* single-char tokens < 256 */

typedef struct {
    const char *src; size_t p; int line;
    tok cur;
    slv *L; pool *pl; int err;
    int depth;          /* parser nesting (see SLV_MAX_DEPTH) */
} parser;

void slv_set_err(slv *L, const char *fmt, ...);
const char *slv_type_name(slv_type t);
void slv_next_tok(parser *P);
void slv_perr(parser *P, const char *msg);

ex *slv_parse_expr(parser *P);
ex *slv_parse_stmt(parser *P);
ex *slv_parse_block(parser *P, const char *until, const char *until2, int *matched);
const char *slv_parse_driver(slv *L, const char *src, pool *pl, ex **out);

/* -------------------------------------------------------------- runtime */

typedef struct env { struct env *parent; slv_table *t; } env;

/* a closure references its defining env; calling sets up a new frame */
struct slv_func {
    ex *body;                  /* N_BLOCK */
    const char *name;
    env *defining;
    int nparams;               /* number of param names */
    const char **params;       /* param names */
};

slv_value slv_eval(slv *L, ex *e, env *en);
slv_value slv_exec(slv *L, ex *e, env *en, int *was_return);
slv_value slv_call_lua(slv *L, slv_func *fn, slv_value *args, int nargs);
slv_value slv_exec_root(slv *L, ex *root);

/* ----------------------------------------------------------------- host */

slv *slv_alloc(void);
void   slv_register_builtins(slv *L);

#endif /* SLV_INTERNAL_H */
