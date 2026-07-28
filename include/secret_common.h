/*
 * secret_common.h — public API for agent/secret_sources/{base,registry}.py.
 *
 * Port of the secret-source contract: a SecretSource resolves external
 * credentials into env-var-shaped values at startup, AFTER ~/.hermes/.env
 * loads and BEFORE the rest of the agent reads os.environ. Read-only,
 * startup-time, synchronous, never raises/prompts.
 *
 * The registry owns everything uniform so no backend can get it wrong:
 * registration (name/scheme uniqueness, API-version gating), per-source
 * wall-clock timeout around fetch(), precedence (mapped > bulk, first-wins),
 * override_existing semantics, protected vars, cross-source conflict
 * warnings, provenance.
 *
 * Opaque-where-possible; sources register a vtable (mirrors plugin_ext.c).
 */
#ifndef SLERMES_SECRET_COMMON_H
#define SLERMES_SECRET_COMMON_H

#include <stddef.h>
#include <stdbool.h>

typedef struct secret_registry secret_registry_t;
typedef struct secret_source secret_source_t;
typedef struct fetch_result fetch_result_t;
typedef struct apply_report apply_report_t;

/* Result of one orchestrated apply pass (port of registry.ApplyReport). */
struct apply_report {
    bool applied_any;        /* any var actually set */
    /* Full provenance/conflict tracking is kept minimal in C; the boolean
     * + applied count is enough for startup gating. */
    size_t applied_count;
    char **conflicts;        /* owned human-readable warnings */
    size_t conflict_count;
};

/* Machine-readable failure taxonomy (port of base.ErrorKind). */
typedef enum {
    SECRET_ERR_NONE = 0,
    SECRET_ERR_NOT_CONFIGURED,
    SECRET_ERR_BINARY_MISSING,
    SECRET_ERR_AUTH_FAILED,
    SECRET_ERR_AUTH_EXPIRED,
    SECRET_ERR_REF_INVALID,
    SECRET_ERR_NETWORK,
    SECRET_ERR_EMPTY_VALUE,
    SECRET_ERR_TIMEOUT,
    SECRET_ERR_INTERNAL
} secret_error_kind_t;

/* Outcome of one source's fetch (port of base.FetchResult). */
struct fetch_result {
    /* name→value the source WOULD contribute (caller-owned strings). */
    char **secret_names;
    char **secret_values;
    size_t secret_count;
    char **warnings;          /* owned */
    size_t warning_count;
    char *error;              /* owned, or NULL */
    secret_error_kind_t error_kind;
    char *binary_path;        /* owned, or NULL (CLI-driven sources) */
};

fetch_result_t *fetch_result_create(void);
void fetch_result_free(fetch_result_t *r);
/* Append a name/value pair (takes ownership of the strings). */
void fetch_result_add(fetch_result_t *r, char *name, char *value);
void fetch_result_add_warning(fetch_result_t *r, const char *msg);
bool fetch_result_ok(const fetch_result_t *r);

/* SecretSource vtable (port of base.SecretSource ABC). */
struct secret_source {
    int api_version;          /* SECRET_SOURCE_API_VERSION */
    const char *name;         /* [a-z0-9_]+, config-section key */
    const char *label;        /* human-readable */
    const char *shape;        /* "mapped" | "bulk" */
    const char *scheme;       /* optional URI scheme this source owns (or NULL) */

    /* Required: resolve secrets. MUST NOT raise/prompt. */
    fetch_result_t *(*fetch)(secret_source_t *self, const char *cfg_json,
                             const char *home_path);
    /* Optional hooks (defaults correct for most sources). */
    bool (*is_enabled)(secret_source_t *self, const char *cfg_json);
    bool (*override_existing)(secret_source_t *self, const char *cfg_json);
    char **(*protected_env_vars)(secret_source_t *self, const char *cfg_json,
                                 size_t *out_count);  /* NULL-terminated, owned */
    double (*fetch_timeout_seconds)(secret_source_t *self, const char *cfg_json);
    /* kind→one-line remediation (pure; must not do I/O). */
    char *(*remediation)(secret_source_t *self, secret_error_kind_t kind);
    void *impl;               /* source-private state */
};

#define SECRET_SOURCE_API_VERSION 1
#define SECRET_DEFAULT_FETCH_TIMEOUT_SECONDS 120.0
#define SECRET_DEFAULT_CLI_TIMEOUT_SECONDS 30.0

/* Run a secret-manager helper CLI with a minimal allowlisted env.
 * argv is a NULL-terminated array (NEVER shell). child gets PATH/HOME/locale
 * basics + allow_env vars + extra_env; NO_COLOR=1; stdin=/dev/null;
 * stderr ANSI-scrubbed. Returns 0 on success (fills out_stdout/out_stderr
 * owned, out_rc), -1 on spawn failure/timeout. Never blocks on stdin. */
int run_secret_cli(char *const argv[], const char *const *allow_env,
                   const char *const *extra_env, double timeout_seconds,
                   char **out_stdout, char **out_stderr, int *out_rc);

/* Strip ANSI escape sequences from text (owned result). */
char *scrub_ansi(const char *text);
/* True when name is a legal env-var name. */
bool is_valid_env_name(const char *name);

/* ── Registry + apply orchestrator (port of registry.py) ─────────────────── */
secret_registry_t *secret_registry_create(void);
void secret_registry_free(secret_registry_t *reg);
/* Register (validates name/shape/api-version/scheme-uniqueness). Takes a
 * borrowed vtable ref (sources live for process lifetime). Returns 0 on ok. */
int secret_registry_register(secret_registry_t *reg, secret_source_t *s);
secret_source_t *secret_registry_get(secret_registry_t *reg, const char *name);
char **secret_registry_list(secret_registry_t *reg);  /* NULL-terminated, owned */

/* Fetch from every enabled source + apply merged result into environ.
 * environ is a NULL-terminated array of "K=V" (caller-owned storage, mutated).
 * Returns an apply_report (owned) describing provenance/conflicts/skips.
 * secrets_cfg_json: the `secrets:` config section as JSON. */
apply_report_t *secret_apply_all(secret_registry_t *reg,
                                 const char *secrets_cfg_json,
                                 const char *home_path,
                                 char **environ);

/* Global registry (lazy singleton). */
secret_registry_t *secret_registry_init(void);
void secret_registry_shutdown(void);

/* A built-in CommandSource (port of command.py) — runs a user script that
 * prints NAME=VALUE lines; fully self-contained, no external binary. */
secret_source_t *secret_command_source_create(void);

#endif /* SLERMES_SECRET_COMMON_H */
