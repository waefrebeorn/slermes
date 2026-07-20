/*
 * cron_scheduler_delivery.h — public API for the cron delivery / origin /
 * mirror / routing helpers ported from cron/scheduler.py.
 *
 * These are the PURE config/routing transforms in scheduler.py that decide
 * WHERE a cron job delivers:
 *   - _resolve_origin              (extract + validate an origin dict)
 *   - _cron_mirror_delivery_enabled (per-job vs global precedence)
 *   - _target_matches_origin       (is a target the job's own origin?)
 *   - _is_known_delivery_platform  (built-in set + plugin registry)
 *   - _resolve_home_env_var        (platform -> home-channel env var)
 *   - _get_home_target_chat_id     (resolve home channel id from env)
 *   - _get_home_target_thread_id   (resolve home thread id from env)
 *   - _iter_home_target_platforms  (built-in + plugin platform names)
 *   - cron_delivery_targets        (configured, connected delivery targets)
 *   - _expand_routing_tokens       ("all" -> connected home platforms)
 *   - _resolve_single_delivery_target / _resolve_delivery_targets /
 *     _resolve_delivery_target     (deliver string -> concrete targets)
 *
 * IO-free on the C side: home channels are read from env vars (set by the
 * harness/oracle), and the connected-platform set is passed in explicitly so
 * the logic is oracle-verifiable without a live gateway. No file lock, no
 * subprocess, no asyncio.
 *
 * Opaque-friendly: the public structs are plain data the caller fills; no
 * god-header needed — only <stddef.h>.
 */

#ifndef CRON_SCHEDULER_DELIVERY_H
#define CRON_SCHEDULER_DELIVERY_H

#include <stddef.h>

/* Origin of a job (the conversation it was scheduled from). */
typedef struct {
    const char *platform;  /* origin platform name, or NULL */
    const char *chat_id;   /* origin chat id, or NULL */
    const char *thread_id; /* origin thread/topic id, or NULL */
    int has_origin;        /* 1 if the job carried an origin dict */
} scheduler_origin_t;

/* Minimal job view the delivery helpers need. */
typedef struct {
    scheduler_origin_t origin;
    const char *deliver;            /* deliver string, or NULL (= "local") */
    int attach_to_session_present;  /* per-job attach_to_session was set */
    int attach_to_session_val;      /* its bool value */
} scheduler_job_t;

/* A resolved concrete delivery target. */
typedef struct {
    char platform[64];
    char chat_id[256];
    char thread_id[256]; /* may be empty */
} scheduler_target_t;

/* A UI-facing delivery target descriptor (cron_delivery_targets). */
typedef struct {
    char id[64];
    char name[128];
    int  home_target_set;
    char home_env_var[128];
} scheduler_delivery_desc_t;

/*
 * Extract + validate a job's origin. Returns 1 and fills *out when the origin
 * is a real dict carrying both platform and chat_id; otherwise returns 0 and
 * leaves *out zeroed. Mirrors _resolve_origin (non-dict origins -> None).
 */
int scheduler_resolve_origin(const scheduler_job_t *job, scheduler_origin_t *out);

/*
 * Whether delivery should mirror into the origin chat's session transcript.
 * Precedence (first decisive wins): per-job attach_to_session bool, then the
 * global mirror flag, then False. Mirrors _cron_mirror_delivery_enabled.
 */
int scheduler_cron_mirror_delivery_enabled(const scheduler_job_t *job,
                                           int global_mirror);

/*
 * True when (platform_name, chat_id, thread_id) is the job's own origin
 * conversation. Mirrors _target_matches_origin: platform compared
 * case-insensitively, chat_id compared exactly, thread_id must match when the
 * origin pins one (else any target thread_id matches).
 */
int scheduler_target_matches_origin(const scheduler_origin_t *origin,
                                    const char *platform_name,
                                    const char *chat_id,
                                    const char *thread_id);

/* Is `name` a valid cron delivery platform? (built-ins + registered plugins). */
int scheduler_is_known_delivery_platform(const char *name);

/* Env var for a platform's home channel (built-in map + plugins). NULL if none.
 * Returned string is static — do not free. */
const char *scheduler_resolve_home_env_var(const char *name);

/* Configured home channel chat id for `name` (env lookup + legacy fallback).
 * Caller frees the result (may be empty string). */
char *scheduler_get_home_target_chat_id(const char *name);

/* Configured home thread id for `name` (env lookup + telegram override +
 * legacy fallback). Caller frees; may return empty string or NULL. */
char *scheduler_get_home_target_thread_id(const char *name);

/*
 * Fill names_out (capacity max) with the built-in + registered plugin platform
 * names that expose a home channel. Returns the count (capped at max).
 */
int scheduler_iter_home_target_platforms(const char **names_out, int max);

/*
 * Expand a routing-intent token to concrete platform names. "all" expands to
 * every home-target platform that currently has a configured home chat id
 * (env-driven). Unknown tokens pass through unchanged. Caller frees each
 * returned string. Returns the count (capped at max).
 */
int scheduler_expand_routing_tokens(const char *part, char **out_names, int max);

/*
 * Resolve one concrete delivery target for deliver_value. Returns 1 and fills
 * *out on success; 0 if the value resolves to no target (e.g. "local", or an
 * unknown platform with no home channel). Mirrors _resolve_single_delivery_target.
 */
int scheduler_resolve_single_delivery_target(const scheduler_job_t *job,
                                             const char *deliver_value,
                                             scheduler_target_t *out);

/*
 * Resolve ALL concrete delivery targets for a job (csv deliver + "all" token,
 * deduped). Fills out (capacity max). Returns the count.
 * Mirrors _resolve_delivery_targets.
 */
int scheduler_resolve_delivery_targets(const scheduler_job_t *job,
                                       scheduler_target_t *out, int max);

/* First resolved delivery target, or 0 if none. Mirrors _resolve_delivery_target. */
int scheduler_resolve_delivery_target(const scheduler_job_t *job,
                                      scheduler_target_t *out);

/*
 * UI-facing delivery targets: every home-target platform that is both a known
 * delivery platform AND in the supplied connected set, reporting whether its
 * home channel is configured. connected[]/n_connected are platform names.
 * Fills out (capacity max). Returns the count. Mirrors cron_delivery_targets.
 */
int scheduler_cron_delivery_targets(const char **connected, int n_connected,
                                    scheduler_delivery_desc_t *out, int max);

/*
 * Register a plugin platform's home-channel env var so the built-in helpers
 * accept it. Returns 1 on success, 0 if the table is full. Replaces any prior
 * entry for the same name.
 */
int scheduler_register_plugin_platform(const char *name, const char *env_var);

#endif /* CRON_SCHEDULER_DELIVERY_H */
