/*
 * profile_store.h — Multi-profile Hermes home directory management.
 *
 * Faithful C11 port of hermes_cli/profiles.py. Each profile is an independent
 * HERMES_HOME (config.yaml, .env, memories, sessions, skills, cron, logs).
 * The "default" profile is slermes_home() itself; named profiles live under
 * <slermes_home()>/profiles/<id>/.
 *
 * Slermes is single-home, so the heavy service-manager / gateway-lifecycle /
 * skills-sync / archive (tar) machinery from the Python module is intentionally
 * a no-op or best-effort here — exactly matching the Python code's
 * "best-effort, swallow errors" behavior for those paths. The pure, testable
 * core (name normalization+validation, path resolution, alias maps, distribution
 * metadata, profile.yaml, sticky active_profile, archive member path-safety,
 * and env resolution) is ported in full.
 *
 * MIT License — Slermes Fork
 */
#ifndef PROFILE_STORE_H
#define PROFILE_STORE_H

#include <stdbool.h>
#include <stddef.h>

/* ── Path helpers ──────────────────────────────────────────────────── */
/* Root where named profiles live: <default_home>/profiles. Caller free(). */
char *profile_profiles_root(void);
/* The default (pre-profile) HERMES_HOME (== slermes_home()). */
char *profile_default_home(void);
/* Path to the sticky active_profile file. Caller free(). */
char *profile_active_profile_path(void);
/* Path to the wrapper-script directory (~/.local/bin). Caller free(). */
char *profile_wrapper_dir(void);

/* ── Validation / normalization ────────────────────────────────────── */
/* Canonical profile id (lowercase; "default" special-cased). Returns malloc'd
 * string, or NULL when name is NULL/empty (mirrors ValueError). */
char *profile_normalize_name(const char *name);
/* Returns true when name is a valid profile identifier ([a-z0-9][a-z0-9_-]{0,63}).
 * Sets *err to malloc'd message on failure (err may be NULL). */
bool profile_validate_name(const char *name, char **err);
/* Returns true when name is a safe wrapper-alias identifier. *err set on fail. */
bool profile_validate_alias_name(const char *name, char **err);

/* ── Resolution ────────────────────────────────────────────────────── */
/* Resolve a profile name to its HERMES_HOME directory. Caller free(). */
char *profile_dir_for(const char *name);
/* True when the HERMES profile directory exists (default always true).
 * This is the hermes_cli/profiles.py semantic (checks the profile's
 * HERMES_HOME dir); DISTINCT from kanban_db.c's profile_exists (which lists
 * kanban profiles). Defined here as profile_dir_exists to avoid the symbol /
 * semantic collision while keeping correct hermes behavior. Returns int (0/1). */
int profile_dir_exists(const char *name);

/* ── Alias / wrapper scripts ───────────────────────────────────────── */
/* Return malloc'd collision message, or NULL if the alias name is safe.
 * PATH-scan for an existing command is omitted (single-home: only our own
 * wrappers live there); reserved-name + subcommand + regex checks remain. */
char *profile_check_alias_collision(const char *name);
/* True when ~/.local/bin is present in PATH. */
bool profile_wrapper_dir_in_path(void);
/* Create a POSIX shell wrapper at ~/.local/bin/<name> invoking hermes -p <profile>.
 * Returns true on success (best-effort: print warning, return false on fail). */
bool profile_create_wrapper_script(const char *name, const char *target);
/* Remove our wrapper for a profile. Returns true if a wrapper was removed. */
bool profile_remove_wrapper_script(const char *name);

/* ── Alias map (single-pass reverse map: canonical_profile -> alias_name) ── */
/* Returns a malloc'd "canon\0alias\ncanon\0alias\n" packed string, or NULL.
 * Parse with profile_alias_map_next(). On a profile-named wrapper the alias
 * equals the canon; a custom-named wrapper wins. Sorted for determinism. */
char *profile_build_alias_map(void);
/* Iterate a packed alias map. *cursor must be NULL on first call; call
 * repeatedly until it returns false. Writes canon/alias (borrowed pointers
 * into the packed buffer). */
bool profile_alias_map_next(char *packed, char **cursor, const char **canon,
                             const char **alias);
/* Find the alias activating profile_name, or NULL. Caller free(). */
char *profile_find_alias_for(const char *profile_name);

/* ── Distribution metadata ─────────────────────────────────────────── */
/* Read <profile_dir>/distribution.yaml -> (name, version, source). Any NULL
 * when missing/unreadable. All returned strings are malloc'd (free each, and
 * the three may be NULL). */
void profile_read_distribution_meta(const char *profile_dir, char **name,
                                    char **version, char **source);

/* ── profile.yaml (per-profile description/role metadata) ──────────── */
/* Read <profile_dir>/profile.yaml. *description and *desc_auto are set.
 * *description is malloc'd (free), "" when absent; *desc_auto is false when
 * absent. Never errors. */
void profile_read_profile_meta(const char *profile_dir, char **description,
                               bool *desc_auto);
/* Write profile.yaml fields (only the provided ones). Creates the file if
 * missing. Returns true on success. */
bool profile_write_profile_meta(const char *profile_dir, const char *description,
                                bool has_description, bool desc_auto,
                                bool has_desc_auto);

/* ── Active profile (sticky default) ───────────────────────────────── */
/* Read the sticky active profile name. Returns malloc'd "default" when
 * unset/empty. Caller free(). */
char *profile_get_active(void);
/* Set the sticky active profile ("default" clears the file). Returns true on
 * success. Validates and (for non-default) requires the profile to exist. */
bool profile_set_active(const char *name);
/* Infer the current profile name from HERMES_HOME: "default" when unset or
 * == default_home; the profile id when under profiles/<id>; else "custom".
 * Returns malloc'd string. Caller free(). */
char *profile_get_active_name(void);

/* ── profile.yaml path ──────────────────────────────────────────── */
/* Returns malloc'd "<profile_dir>/profile.yaml". Caller free(). */
char *profile_yaml_path(const char *profile_dir);

/* ── Archive member path-safety (import/export) ────────────────────── */
/* Validate a tar member path. Returns true when safe (no absolute, no "..",
 * no drive, non-empty parts). Mirrors _normalize_profile_archive_parts. */
bool profile_archive_member_safe(const char *member);

/* ── Bundled-skills opt-out ───────────────────────────────────────── */
/* True when the profile root holds the .no-bundled-skills marker. Fails open
 * (false) on any OS error, mirroring has_bundled_skills_opt_out. */
bool profile_has_bundled_skills_opt_out(const char *profile_dir);

/* ── Skills count (cached signature scan) ─────────────────────────── */
double profile_skills_dir_signature(const char *skills_dir);
int profile_count_skills(const char *profile_dir);

/* ── config.yaml model/provider read ──────────────────────────────── */
/* Read model/provider from a profile's config.yaml. *model/*provider set to
 * malloc'd strings or NULL. Free each (NULL-safe). */
void profile_read_config_model(const char *profile_dir, char **model,
                               char **provider);

/* ── profiles_to_serve (gateway multiplexing set) ─────────────────── */
/* Build the (name, home) pairs a gateway should serve. multiplex=false ->
 * single active profile; true -> default + all valid named profiles.
 * Returns a malloc'd packed "name\0home\nname\0home\n" buffer (borrow with
 * profile_serve_next). Caller free(). NULL on failure. */
char *profile_profiles_to_serve(bool multiplex);
bool profile_serve_next(char *packed, char **cursor, const char **name,
                        const char **home);

/* ── Clone/export ignore predicates (copytree ignore) ─────────────── */
/* Should `entry` (direct child `dir` of `source_dir`) be excluded from a
 * --clone-all copy? Mirrors _clone_all_copytree_ignore (history artifacts at
 * any root; default-only infra gated on is_default_source; universal
 * pycache/sock/tmp at any depth). */
bool profile_clone_ignore(const char *source_dir, const char *dir,
                           const char *entry);
/* Same shape for export archives (broader root exclusion set). Mirrors
 * _default_export_ignore. */
bool profile_export_ignore(const char *root_dir, const char *dir,
                            const char *entry);

/* ── .env backfill ────────────────────────────────────────────────── */
/* Copy the default install's .env into every named profile lacking one.
 * Returns malloc'd newline-joined list of backfilled profile names (free),
 * "" when none. NULL on error. */
char *profile_backfill_profile_envs(void);

/* ── Seed bundled skills ──────────────────────────────────────────── */
/* Single-home Slermes has no skills-sync subprocess; faithful fail-open:
 * returns malloc'd JSON-ish result dict ("copied"/"updated" empty,
 * "skipped_opt_out" when opted out) or NULL on hard error. Caller free(). */
char *profile_seed_profile_skills(const char *profile_dir, bool quiet);

/* ── Config schema migration (no-op in single-home) ───────────────── */
/* Mirrors _migrate_profile_config_if_outdated: single-home has no separate
 * config-schema pipeline, so this is a safe no-op that never fails. */
void profile_migrate_config_if_outdated(const char *profile_dir);

/* ── Gateway service lifecycle (host no-ops) ─────────────────────── */
/* These mirror the s6/service-manager hooks. On a host (systemd/launchd/
 * windows) — which is every Slermes deployment — they are silent no-ops,
 * exactly matching the upstream host short-circuit. They never error. */
void profile_maybe_register_gateway_service(const char *profile_name);
void profile_maybe_unregister_gateway_service(const char *profile_name);
void profile_cleanup_gateway_service(const char *name, const char *profile_dir);

/* Stop a running gateway by reading its gateway.pid and SIGTERM-ing it,
 * polling up to 10s for exit. Faithful port of _stop_gateway_process. */
void profile_stop_gateway_process(const char *profile_dir);

/* ── Archive import/export helpers ────────────────────────────────── */
/* Inspect a .tar.gz archive's top-level directory names. Returns malloc'd
 * newline-joined set (free), "" when none, NULL on error. Mirrors
 * _inspect_profile_archive_roots. */
char *profile_inspect_archive_roots(const char *archive_path);
/* Extract a .tar.gz archive into destination, rejecting any member whose
 * normalized path escapes (absolute / ".." / drive). Returns true on success.
 * Minimal, dependency-free tar+gz reader (zlib). Mirrors
 * _safe_extract_profile_archive. */
bool profile_safe_extract_archive(const char *archive_path,
                                  const char *destination);

/* ── Honcho host rename (single-home: no-op) ─────────────────────── */
/* Faithful no-op: single-home Slermes has no Honcho supervisor config to
 * migrate on rename. */
void profile_migrate_honcho_profile_host(const char *old_name,
                                          const char *new_name,
                                          const char *new_dir);

/* ── Gateway-running probe ─────────────────────────────────────────── */
/* Check if a gateway is running for a profile dir, using the same
 * gateway.pid + gateway_state.json liveness logic as gateway/status.py.
 * Returns true when a live gateway is detected. */
bool profile_gateway_running(const char *profile_dir);

/* ── Profile env resolution ────────────────────────────────────────── */
/* Resolve a profile name to its HERMES_HOME path string. Caller free().
 * Returns NULL when the profile doesn't exist (non-default). */
char *profile_resolve_env(const char *profile_name);

#endif /* PROFILE_STORE_H */
