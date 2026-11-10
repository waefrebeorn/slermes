/*
 * slv_internal.h -- private interface of slerm-Lua.
 *
 * Included only by the slv*.c translation units. The public header keeps
 * slv / slv_value / slv_func / slv_table opaque; this is where the
 * state, the AST, the lexer and the parser live, so no caller can reach in.
 */
#ifndef SLV_INTERNAL_H
#define SLV_INTERNAL_H

#include "slv.h"

#include <setjmp.h>
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
    N_NIL, N_TRUE, N_FALSE, N_INT, N_NUM, N_STR, N_VARARG, N_NAME,
    N_INDEX, N_CALL, N_METHOD, N_FUNC, N_TABLE,
    N_BINOP, N_UNARY, N_AND, N_OR, N_PAREN,
    N_LOCAL, N_ASSIGN, N_LOCALFUNC, N_FUNCSTAT,
    N_IF, N_WHILE, N_REPEAT, N_FORNUM, N_FORIN, N_DO,
    N_RETURN, N_BREAK, N_GOTO, N_LABEL, N_BLOCK,
} ntype;

typedef struct ex {
    ntype t; int line;
    int64_t i; double num; const char *str; const char *name;
    int op;                          /* operator token for N_BINOP/N_UNARY  */
    struct ex *e1, *e2, *e3, *e4;    /* operands / cond / body / else / step */
    struct ex **list; int nlist;     /* stmts / args / params / fields       */
    struct ex **keys;                /* table: {list=values, keys=key-expr}  */
    const char **names;              /* param names / loop variables         */
    const char *label;               /* goto / label name                    */
    int is_method;
} ex;

#define SLV_MAX_EX 65536
/* Lua's own limits. All three are independent: the parser can recurse
 * iteratively (a postfix chain) while eval recurses per node. */
#define SLV_MAX_DEPTH 200
#define SLV_MAX_EVAL_DEPTH 200
#define SLV_MAX_CALL 190

typedef struct { ex n[SLV_MAX_EX]; int cnt; } pool;

/* ---------------------------------------------------------------- state */

struct slv_multi { int n; slv_value v[]; };

struct slv {
    struct anode *root;
    struct istr **buckets; size_t nbuckets;
    slv_table *globals;
    slv_table *string_meta;        /* metatable for the string type        */
    char err[512]; int has_err;
    int curline;                     /* line of the node being executed      */
    const char *chunkname;
    int depth;                       /* Lua call nesting (recursion guard)   */
    int eval_depth;
    pool *pool;

    /* error isolation: a stack of catch points for pcall/xpcall */
    jmp_buf *jmp; int njmp;
    /* value stack, used for multiple returns and call frames */
    slv_value *stk; int sp, scap;

    /* non-local control: goto */
    const char *goto_label; int has_goto;

    /* varargs of the currently executing Lua frame (saved/restored) */
    slv_value *va; int nva;

    uint64_t rand_state[2];
};

/* --------------------------------------------------------- string pool */

typedef struct istr { char *s; size_t len; unsigned hash; struct istr *next; } istr;

/* --------------------------------------------------------------- tables */

typedef struct slv_kv { slv_value k, v; int used; } slv_kv;
struct slv_table {
    slv_kv *e; size_t cap, count;
    struct slv_table *meta;
    size_t seq;                      /* cached array-part length for `#`     */
    int seq_dirty;
};

/* --------------------------------------------------------------- parser */

/* char tokens are (unsigned char)c, always < 256; the rest are >= 256. */
typedef enum {
    T_EOF = 0,
    T_INT = 256, T_NUM, T_STR, T_NAME,
    /* keywords */
    T_AND, T_BREAK, T_DO, T_ELSE, T_ELSEIF, T_END, T_FALSE, T_FOR,
    T_FUNCTION, T_GOTO, T_IF, T_IN, T_LOCAL, T_NIL, T_NOT, T_OR, T_REPEAT,
    T_RETURN, T_THEN, T_TRUE, T_UNTIL, T_WHILE,
    /* compound */
    T_CONCAT, T_DOTS, T_EQ, T_NE, T_LE, T_GE, T_IDIV, T_SHL, T_SHR, T_DBCOLON,
} tktype;

typedef struct { int type; int64_t i; double num; const char *str; int line; } tok;

#define SLV_TKCHAR(c) ((int)(unsigned char)(c))

typedef struct {
    const char *src; size_t p; int line;
    tok cur;
    slv *L; pool *pl; int err;
    int depth;
} parser;

void slv_set_err(slv *L, const char *fmt, ...);
void slv_error(slv *L, const char *fmt, ...);   /* raises: longjmps or sets */
const char *slv_type_name(slv_type t);
void slv_next_tok(parser *P);
void slv_perr(parser *P, const char *msg);

ex *slv_parse_expr(parser *P);
ex *slv_parse_stmt(parser *P);
ex *slv_parse_block(parser *P, int *matched);
const char *slv_parse_driver(slv *L, const char *src, pool *pl, ex **out);

/* -------------------------------------------------------------- runtime */

typedef struct env { struct env *parent; slv_table *t; } env;

struct slv_func {
    ex *body;                  /* N_BLOCK                                    */
    const char *name;
    env *defining;
    int nparams;               /* number of param names                      */
    const char **params;
    slv_value *varargs; int nvarargs;
};

slv_value slv_eval(slv *L, ex *e, env *en);
slv_value slv_eval_multi(slv *L, ex *e, env *en, slv_value **out, int *nout);
slv_value slv_exec(slv *L, ex *e, env *en, int *was_return);
slv_value slv_call_lua(slv *L, slv_func *fn, slv_value *args, int nargs);
int         slv_call_lua_results(slv *L, slv_func *fn, slv_value *args, int nargs);
int         slv_call_value(slv *L, slv_value fn, slv_value *args, int nargs, slv_value **out);
slv_value slv_multi_value(slv *L, slv_value *v, int n);
slv_value slv_exec_root(slv *L, ex *root);

/* operators with metamethod dispatch */
slv_value slv_arith(slv *L, int op, slv_value a, slv_value b);
slv_value slv_compare(slv *L, int op, slv_value a, slv_value b);
int         slv_len(slv *L, slv_value v, size_t *out);
slv_value slv_concat(slv *L, slv_value a, slv_value b);
slv_value slv_index(slv *L, slv_value obj, slv_value key);
void        slv_setindex(slv *L, slv_value obj, slv_value key, slv_value v);
size_t      slv_table_len(const slv_table *t);
int         slv_table_next(slv_table *t, slv_value *k, slv_value *v);

/* catch points for pcall */
jmp_buf *slv_push_catch(slv *L);
void     slv_pop_catch(slv *L);

/* -------------------------------------------------------------- host */

void slv_register_builtins(slv *L);
void slv_register_string(slv *L);
void slv_register_table(slv *L);
void slv_register_math(slv *L);
void slv_register_os(slv *L);
void slv_register_io(slv *L);

#endif /* SLV_INTERNAL_H */
