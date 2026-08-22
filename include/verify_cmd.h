/*
 * verify_cmd.h — Port of hermes_cli/verify_cmd.py ("hermes verify").
 *
 * Detects a project's run recipe and smoke-tests it. The heavy lifting
 * (recipe detection, phase execution, readiness poll, evidence ledger) lives
 * in agent.verify / agent.coding_context / agent.verification_evidence —
 * live subsystems the C port reaches through injectable hooks, while the
 * CLI orchestration, the project-facts merge, the evidence-tail building and
 * the human report printer are ported faithfully here.
 */
#ifndef VERIFY_CMD_H
#define VERIFY_CMD_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: agent.verify Recipe (the fields verify_cmd.py touches) */
typedef struct {
    char *name;      /* owned */
    char *kind;      /* owned */
    char *start;     /* owned */
    char **bootstrap; size_t n_bootstrap;   /* owned strings */
    char **build;     size_t n_build;
    char **test;      size_t n_test;
    long port;
} vc_recipe_t;
vc_recipe_t *vc_recipe_new(void);
void vc_recipe_free(vc_recipe_t *r);
vc_recipe_t *vc_recipe_clone(const vc_recipe_t *r);
void vc_recipe_append_test(vc_recipe_t *r, const char *command);

/* PoP: agent.verify PhaseResult */
typedef struct {
    char *phase;        /* owned */
    char *command;      /* owned */
    int ok;
    int timed_out;
    double duration;
    char *output_tail;  /* owned, may be NULL/empty */
} vc_phase_t;

/* PoP: agent.verify ReadinessResult */
typedef struct {
    int present;
    int ready;
    long status_code;
    char *url;          /* owned */
    char *error;        /* owned, may be empty */
    char *output_tail;  /* owned */
    double duration;
} vc_readiness_t;

/* PoP: agent.verify VerifyResult */
typedef struct {
    int ok;
    vc_phase_t *phases; size_t n_phases;
    vc_readiness_t readiness;
} vc_result_t;
vc_result_t *vc_result_new(void);
void vc_result_free(vc_result_t *r);

/* CLI args (argparse surface of `hermes verify`) */
typedef struct {
    const char *path;          /* getattr(args,"path",None) or "." */
    int path_ok;               /* root.is_dir() result (1 unless scripted) */
    int json;                  /* --json */
    int save;                  /* --save */
    int detect_only;           /* --detect-only */
    int skip_start;            /* --skip-start */
    long port;                 /* --port (0 = not given) */
    const char *const *phases; size_t n_phases;  /* --phase (repeatable) */
    double timeout;            /* --timeout */
    double ready_timeout;      /* --ready-timeout */
} verify_cmd_args_t;

/* Injectable seams over agent.verify / agent.coding_context /
 * agent.verification_evidence. load_or_detect returns 0 and a borrowed
 * recipe + source ("manifest"/"detected"), or 1 for "no recipe". */
typedef struct verify_cmd_hooks {
    int (*load_or_detect)(const char *root, vc_recipe_t **out_recipe,
                          const char **out_source, void *ctx);
    const char *(*manifest_path)(const char *root, void *ctx); /* borrowed */
    const char *(*save_manifest)(const char *root, vc_recipe_t *recipe,
                                 void *ctx); /* borrowed path */
    void (*run_verify)(const char *root, vc_recipe_t *recipe,
                       const char *const *phases, size_t n_phases,
                       double phase_timeout, double ready_timeout,
                       int skip_start, long port_override,
                       vc_result_t **out_result, void *ctx);
    /* PoP: agent.coding_context.detect_project_facts(root).verify_commands */
    const char *const *(*detect_facts_commands)(const char *root, size_t *n,
                                                void *ctx);
    /* PoP: agent.verification_evidence.record_verify_run(...) */
    void (*record_verify_run)(const char *root, const char *session_id,
                              int ok, const char *command, const char *scope,
                              const char *output, void *ctx);
    void *ctx;
} verify_cmd_hooks_t;

/* PoP: run_verify_command @ hermes_cli/verify_cmd.py — returns the CLI exit
 * code; prints to stdout/stderr exactly like the Python. */
int verify_cmd_run_verify_command(const verify_cmd_args_t *args,
                                  const verify_cmd_hooks_t *hooks,
                                  const char *session_id);

/* PoP: _merge_project_facts_commands — best-effort union of project-facts
 * verify commands into a detected recipe's test list (never applied to a
 * saved manifest; never raises). */
void verify_cmd_merge_project_facts_commands(vc_recipe_t *recipe,
                                             const char *const *commands,
                                             size_t n_commands);

/* PoP: _record_evidence — builds the output tails and hands them to
 * record_verify_run (best-effort, fail-silent). `partial` downgrades scope
 * to "targeted". Returns the malloc'd joined output (caller frees) so
 * oracles can observe it even when the ledger hook is absent. */
char *verify_cmd_record_evidence(const char *root, const vc_recipe_t *recipe,
                                 const vc_result_t *result, int partial,
                                 const verify_cmd_hooks_t *hooks,
                                 const char *session_id);

/* PoP: _print_human_report */
void verify_cmd_print_human_report(const vc_recipe_t *recipe,
                                   const char *source,
                                   const vc_result_t *result, FILE *out);

#ifdef __cplusplus
}
#endif

#endif /* VERIFY_CMD_H */
