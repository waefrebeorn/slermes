/*
 * credential_sources.h — Unified removal contract for credential sources.
 * AG30: Port of Python agent/credential_sources.py (448 lines).
 *
 * Each credential source registers a RemovalStep defining how to clean up
 * external state and suppress re-seeding. The registry is searched in order;
 * first match wins.
 */

#ifndef CREDENTIAL_SOURCES_H
#define CREDENTIAL_SOURCES_H

#include "hermes_json.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REMOVAL_RESULT_MAX_LINES 16
#define REMOVAL_MAX_STEPS 32

/* ================================================================
 *  RemovalResult — outcome of removing a credential source
 * ================================================================ */

typedef struct {
    char *cleaned[REMOVAL_RESULT_MAX_LINES];  /* External state that was mutated */
    int   cleaned_count;
    char *hints[REMOVAL_RESULT_MAX_LINES];    /* Diagnostic hints for user */
    int   hints_count;
    bool  suppress;                           /* Whether to suppress re-seed */
} removal_result_t;

void removal_result_init(removal_result_t *r);
void removal_result_add_cleaned(removal_result_t *r, const char *msg);
void removal_result_add_hint(removal_result_t *r, const char *msg);
void removal_result_free(removal_result_t *r);

/* ================================================================
 *  RemovalStep — how to remove one credential source
 * ================================================================ */

/* Match function: returns true if this step handles the given source */
typedef bool (*removal_match_fn_t)(const char *source);

/* Remove function: performs cleanup, returns result */
typedef removal_result_t (*removal_fn_t)(const char *provider, const char *source);

typedef struct {
    const char         *provider;    /* Provider pool key, or "*" for any */
    const char         *source_id;   /* Source identifier */
    removal_match_fn_t  match_fn;    /* Optional: overrides literal source_id match */
    removal_fn_t        remove_fn;   /* Performs cleanup */
    const char         *description; /* Human-readable description */
} removal_step_t;

/* ================================================================
 *  Registry API
 * ================================================================ */

/* Register a removal step. Call credential_sources_init() to register all built-in steps. */
void credential_sources_register(const removal_step_t *step);

/* Find the first matching removal step for a provider+source pair.
 * Port of Python agent/credential_sources.py:find_removal_step(). */
const removal_step_t *find_removal_step(const char *provider, const char *source);

/* Initialize the built-in removal step registry. Call once at startup. */
void credential_sources_init(void);

/* ================================================================
 *  Helper: suppress a credential source in auth.json
 * ================================================================ */
void suppress_credential_source(const char *provider, const char *source);

/* Helper: remove an env var from ~/.hermes/.env and process environment */
bool remove_env_value(const char *env_var);

#ifdef __cplusplus
}
#endif

#endif /* CREDENTIAL_SOURCES_H */
