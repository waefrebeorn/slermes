#ifndef HERMES_TERMINAL_ENV_PROVIDER_H
#define HERMES_TERMINAL_ENV_PROVIDER_H

/*
 * terminal_env_provider.h — pluggable terminal execution backend ABC.
 *
 * Port of Python agent/terminal_env_provider.py:TerminalEnvironmentProvider.
 * The Python ABC maps to a vtable struct (slermes-vtable-abc-polymorphism
 * pattern): abstract methods are function pointers; concrete providers fill
 * them at registration. Optional hooks left NULL fall back to the same
 * defaults the Python base class provides (NULL == NotImplementedError for
 * the two @abstractmethod slots, and NULL optional hook = base default).
 *
 * Classification contract (see py module docstring):
 *   is_remote, is_container, session_isolated_when_nonpersistent,
 *   skip_container_guards (defaults to is_container), cache_path_base,
 *   strip_env_keys, env_description.
 */

#include <stdbool.h>
#include <stddef.h>

typedef struct json_t json_t;

/* doctor_checks row: (ok, label, detail). Caller frees strings. */
typedef struct {
    bool ok;
    char *label;
    char *detail;
} tep_doctor_row_t;

/* probe() result: (status, detail). Caller frees strings. */
typedef struct {
    char *status;   /* "ready" | "needs_setup" | "unavailable" */
    char *detail;
} tep_probe_t;

/* create_environment keyword args, flattened. */
typedef struct {
    const char *cwd;
    int timeout;
    const char *task_id;        /* "default" when unset */
    const char *image;          /* may be NULL */
    const json_t *container_config; /* may be NULL */
} tep_create_args_t;

typedef struct terminal_env_provider_s terminal_env_provider_t;

struct terminal_env_provider_s {
    /* ── Identity ──────────────────────────────────────────────────── */
    /* Abstract: stable short id ("terminal.backend"/TERMINAL_ENV value).
     * NULL vtable slot == abstract method not implemented. */
    const char *(*name)(terminal_env_provider_t *self);

    /* Optional: defaults to name(). */
    const char *(*display_name)(terminal_env_provider_t *self);

    /* Optional: defaults to
     * "Run commands in a <display_name> environment.". */
    const char *(*description)(terminal_env_provider_t *self);

    /* ── Classification flags ──────────────────────────────────────── */
    bool is_remote;                             /* default true  */
    bool is_container;                          /* default true  */
    bool session_isolated_when_nonpersistent;   /* default false */

    /* Optional: skip_container_guards. Defaults to is_container. */
    bool (*skip_container_guards_fn)(terminal_env_provider_t *self);

    /* Optional: cache_path_base. Default NULL (= host paths correct). */
    const char *(*cache_path_base)(terminal_env_provider_t *self);

    /* Optional: strip_env_keys → NULL-terminated argv-style list of
     * credential env var names. Default empty (NULL return). */
    const char *const *(*strip_env_keys)(terminal_env_provider_t *self);

    /* Optional: prompt-builder fallback description. Default:
     * "a <display_name> environment (likely Linux)". */
    const char *(*env_description)(terminal_env_provider_t *self);

    /* ── Availability / setup UX ───────────────────────────────────── */
    /* Abstract: cheap availability check — no network calls. */
    bool (*is_available)(terminal_env_provider_t *self);

    /* Optional: full requirements check over merged terminal-env config
     * dict. Default defers to is_available(). Return false + log. */
    bool (*check_requirements)(terminal_env_provider_t *self,
                               const json_t *config);

    /* Optional: dashboard picker probe. Default:
     * available ? ("ready","") : ("needs_setup",
     *                              "<display_name> is not configured.") */
    void (*probe)(terminal_env_provider_t *self, tep_probe_t *out);

    /* Optional: setup-instruction lines. Default empty list (NULL). */
    const char *const *(*setup_instructions)(terminal_env_provider_t *self);

    /* Optional: interactive post-setup hook. Default no-op. */
    void (*post_setup)(terminal_env_provider_t *self);

    /* Optional: hermes doctor rows (NULL-terminated array). Default:
     * single row reflecting is_available(). Caller frees rows+strings. */
    tep_doctor_row_t *(*doctor_checks)(terminal_env_provider_t *self);

    /* ── The factory ───────────────────────────────────────────────── */
    /* Abstract: create an execution environment. Must tolerate unknown
     * future fields in tep_create_args_t (forward-compat contract). */
    void *(*create_environment)(terminal_env_provider_t *self,
                                const tep_create_args_t *args);

    /* Opaque per-provider state (plugin data). Never touched here. */
    void *ctx;
};

/* ── Base-class default implementations (port of the @property bodies) ── */

const char *tep_default_display_name(terminal_env_provider_t *self);
const char *tep_default_description(terminal_env_provider_t *self);
bool tep_default_skip_container_guards(terminal_env_provider_t *self);
const char *tep_default_env_description(terminal_env_provider_t *self);
bool tep_default_check_requirements(terminal_env_provider_t *self,
                                    const json_t *config);
void tep_default_probe(terminal_env_provider_t *self, tep_probe_t *out);
tep_doctor_row_t *tep_default_doctor_checks(terminal_env_provider_t *self);

/* Dispatch helpers: apply the base defaults when the slot is NULL, exactly
 * like Python's inherited @property bodies. These are what callers use. */
const char *tep_name(terminal_env_provider_t *self);
const char *tep_display_name(terminal_env_provider_t *self);
const char *tep_description(terminal_env_provider_t *self);
bool tep_skip_container_guards(terminal_env_provider_t *self);
const char *tep_cache_path_base(terminal_env_provider_t *self); /* NULL = none */
const char *const *tep_strip_env_keys(terminal_env_provider_t *self);
const char *tep_env_description(terminal_env_provider_t *self);
bool tep_check_requirements(terminal_env_provider_t *self,
                            const json_t *config);
void tep_probe(terminal_env_provider_t *self, tep_probe_t *out);
tep_doctor_row_t *tep_doctor_checks(terminal_env_provider_t *self);
void *tep_create_environment(terminal_env_provider_t *self,
                             const tep_create_args_t *args);

#endif /* HERMES_TERMINAL_ENV_PROVIDER_H */
