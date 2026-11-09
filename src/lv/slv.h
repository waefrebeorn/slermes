/*
 * slv.h -- slerm-Lua: the C11 tree-walking Lua interpreter.
 *
 * WHY: stock Lua is the placeholder to get the engine running. slerm-Lua
 * exists for C11 SPEED + control -- it is the substrate that lets people
 * write the best mods and lets other games be slermed INTO our world space,
 * a distribution surface the world boots in WuBuOS.
 *
 * THIS milestone is the semantic foundation: a clean tree-walking subset
 * interpreter that meets the wb_script contract (bind world, call wb.*, run
 * step). Correctness + portability are the goals here -- NOT yet speed. The
 * register bytecode VM that actually beats stock Lua is the NEXT milestone;
 * that benchmark claim is made only when it exists.
 *
 * Subset: nil/bool/number/string(interned)/table/function; local + assign;
 * if/else; while; numeric for; table literals; index t[k]/t.k; method t:f();
 * calls (lua + host cfn); return. No coroutines/metamethods/varargs.
 */
#ifndef SLV_H
#define SLV_H

#include <stdint.h>
#include <stddef.h>

typedef struct slv       slv;
typedef struct slv_value slv_value;
typedef struct slv_func  slv_func;
typedef struct slv_table slv_table;

typedef slv_value (*slv_cfn)(slv *L, slv_value *args, int nargs, void *ud);

/* --------------------------------------------------------------- values */
typedef enum {
    SLV_NIL = 0, SLV_BOOL, SLV_NUM, SLV_STR, SLV_TABLE,
    SLV_FUNCTION, SLV_CFUNCTION,
} slv_type;

struct slv_value {
    slv_type t;
    union {
        int         b;
        double      n;
        const char *s;
        slv_table *tab;
        slv_func  *fn;
        struct { slv_cfn f; void *ud; } cf;
    } as;
};

#define slv_nil()     ((slv_value){.t=SLV_NIL})
#define slv_bool(B)   ((slv_value){.t=SLV_BOOL, .as.b=(B)})
#define slv_number(N) ((slv_value){.t=SLV_NUM,  .as.n=(N)})
#define slv_str(S)    ((slv_value){.t=SLV_STR,  .as.s=(S)})

int slv_truthy(slv_value v);
int slv_equal(slv_value a, slv_value b);

/* --------------------------------------------------------------- state */
slv *slv_new(void);
void   slv_free(slv *L);

const char *slv_intern(slv *L, const char *bytes, size_t len);
double      slv_tonumber(slv_value v, int *ok);
const char *slv_tostring(slv_value v);

/* --------------------------------------------------------------- tables */
slv_table *slv_table_new(slv *L, int narr, int nrec);
void         slv_table_set(slv *L, slv_table *t, slv_value k, slv_value v);
slv_value  slv_table_get(slv *L, const slv_table *t, slv_value k);

/* --------------------------------------------------------------- driver */
const char *slv_run(slv *L, const char *src, const char *chunkname);
void        slv_register_global(slv *L, const char *name, slv_cfn fn, void *ud);
slv_value slv_get_global(slv *L, const char *name);
void        slv_set_global(slv *L, const char *name, slv_value v);

/* host-driven call: invoke a function value (e.g. step) with args */
slv_value slv_call(slv *L, slv_value fn, slv_value *args, int nargs);

#endif /* SLV_H */
