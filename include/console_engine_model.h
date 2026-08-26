/* console_engine_model.h — shared model types for the C11 port of
 * hermes_cli/console_engine.py (HermesConsoleEngine + parser-tree helpers).
 *
 * The Python module leans on argparse trees, importlib and namespaces; here
 * those become small plain-C structures defined in
 * src/hermes_cli/console_engine_pure.c and shared with
 * src/hermes_cli/console_engine_ports.c.
 */
#ifndef SLERMES_CONSOLE_ENGINE_MODEL_H
#define SLERMES_CONSOLE_ENGINE_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* argparse.SUPPRESS sentinel (matches ce_clean_summary's convention). */
#define CE_SUPPRESS "==SUPPRESS=="

/* ── Namespace (argparse.Namespace) ─────────────────────────────────── */
typedef struct ce_ns {
    char **keys;
    char **vals; /* malloc'd strings; "true"/"false" for flags */
    size_t n, cap;
} ce_ns_t;

void ce_ns_init(ce_ns_t *ns);
void ce_ns_set(ce_ns_t *ns, const char *key, const char *val);
const char *ce_ns_get(const ce_ns_t *ns, const char *key);
bool ce_ns_has(const ce_ns_t *ns, const char *key);
void ce_ns_free(ce_ns_t *ns);

/* ── Parser tree (mini-argparse over the shapes console_engine uses) ── */
typedef struct ce_choice_item {
    char *dest;
    char *metavar;
    char *help; /* NULL or CE_SUPPRESS means suppressed */
} ce_choice_item;

typedef struct ce_parser ce_parser;

typedef struct ce_action {
    int kind; /* 0 positional, 1 optional, 2 subparsers */
    char *dest;
    char *metavar;
    char *help;
    char *type_name;   /* "int" or NULL */
    char *deflt;       /* default value string, or NULL when absent */
    bool has_default;
    bool nargs_plus;   /* nargs="+" */
    bool store_true;   /* action="store_true" */
    /* subparsers only */
    char **sub_names;
    ce_parser **sub_parsers;
    size_t n_subs, sub_cap;
    ce_choice_item *choice_actions;
    size_t n_choices, choice_cap;
} ce_action;

struct ce_parser {
    char *prog;
    char *description;
    ce_action **actions;
    size_t n_actions, act_cap;
    /* set_defaults() storage (includes "func" symbol ids) */
    char **def_keys;
    char **def_vals;
    size_t n_defs, def_cap;
};

/* ── Handler command object (the *_handler factories' closures) ─────── */
typedef struct ce_hcmd {
    int kind; /* 1 extracted, 2 registered, 3 builder, 4 adder */
    char *root;
    char **fixed;
    size_t n_fixed;
    char *module;
    char *name1; /* builder_name / register_name / add_name */
    char *name2; /* main_handler_name / handler_name / NULL */
} ce_hcmd;

/* namespace_update callback: 0 ok, -1 raised ConsoleCommandError (*err). */
typedef int (*ce_ns_update_fn)(ce_ns_t *ns, char **err);

/* ── Engine ─────────────────────────────────────────────────────────── */
struct coe_engine;
/* Simple registered handler: returns malloc'd output text, prints via the
 * captured streams, or sets *err (malloc'd ConsoleCommandError message). */
typedef char *(*coe_simple_fn)(struct coe_engine *eng, char **args,
                               size_t nargs, char **err);

typedef struct coe_cmd {
    char *key;      /* space-joined path tuple */
    char *usage;
    char *summary;
    void *handler;
    int kind;       /* 0 = ce_hcmd*, 1 = coe_simple_fn */
    bool mutating;
    char *confirmation;
} coe_cmd;

typedef struct coe_engine {
    long output_limit;
    char **history;
    size_t n_hist, hist_cap;
    coe_cmd *cmds;
    size_t n_cmds, cmds_cap;
} coe_engine;

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_CONSOLE_ENGINE_MODEL_H */

/* promoted from console_engine_pure.c statics for cross-TU use */
char *ce_strip_heap(const char *s);
void ce_rstrip_inplace(char *s);
char **coe_close_matches(const char *probe, char **available, size_t navail,
                         size_t n, double cutoff, size_t *out_n);
