/*
 * port_hermes_cli_update_cmd.h — C11 port of hermes_cli/update_cmd.py.
 *
 * Ports the pure, deterministic helpers lifted out of
 * hermes_cli/update_cmd.py so they are oracle-verifiable against the
 * Python originals. The heavy update orchestration (git pull, zip
 * download, venv refresh, npm install, gateway restart) lives in the
 * existing cmd_update() / port_web_update.c and shells out through the
 * shared git subprocess layer — this header only covers the pure logic
 * that transforms inputs to outputs with no I/O.
 *
 * Memory: string-returning functions return malloc'd strings (caller
 * frees) or NULL. All other functions return a value by value.
 */

#ifndef PORT_HERMES_CLI_UPDATE_CMD_H
#define PORT_HERMES_CLI_UPDATE_CMD_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── hermes_cli/update_cmd.py pure helpers ────────────────────────────── */

/* PoP: _format_time_ago @ hermes_cli/update_cmd.py:_format_time_ago
 * Render an ISO-8601 timestamp as "5m ago" / "3h ago" / "2d ago" / "just now".
 * Best-effort: returns "recently" on any parse failure. Caller does NOT free. */
const char *uc_format_time_ago(const char *iso_ts);

/* PoP: _stash_apply_failed_only_on_existing_untracked @ hermes_cli/update_cmd.py:_stash_apply_failed_only_on_existing_untracked
 * True when a `git stash apply` failure stderr is ONLY about untracked files
 * that already exist in the working tree (the permission-denied autostash
 * tail-end). Any other error line => False. */
bool uc_stash_apply_failed_only_on_existing_untracked(const char *stderr_text);

/* PoP: _is_fork @ hermes_cli/update_cmd.py:_is_fork
 * True when origin_url is NOT one of the official NousResearch URLs.
 * Normalizes by stripping a trailing "/" and trailing ".git" before compare. */
bool uc_is_fork_origin(const char *origin_url);

/* PoP: _service_restart_sec @ hermes_cli/update_cmd.py:_service_restart_sec
 * Parse a systemd RestartUSec value ("30s", "100ms", "1min 30s", "infinity")
 * into seconds. Returns `default` on any parse miss or "infinity". */
double uc_parse_restart_sec(const char *raw, double default_sec);

/* PoP: _print_concurrent_instances_message @ hermes_cli/update_cmd.py:_format_concurrent_instances_message
 * Build the human-readable blocker message for running hermes.exe instances.
 * Returns a malloc'd string (caller frees). matches is an array of "pid\tname"
 * tab-separated strings; n_matches is the count. scripts_dir is the path used
 * to locate hermes.exe. */
char *uc_format_concurrent_instances_message(const char **matches,
                                             size_t n_matches,
                                             const char *scripts_dir);

/* PoP: _resolve_stash_selector @ hermes_cli/update_cmd.py:_resolve_stash_selector
 * Given `git stash list` output (one "selector commit" line per entry) and a
 * target commit SHA, return the matching stash selector (e.g. "stash@{1}")
 * or NULL if not found. Returns a malloc'd string (caller frees). */
char *uc_resolve_stash_selector(const char *stash_list_output,
                                const char *stash_ref);

/* PoP: _print_stash_cleanup_guidance @ hermes_cli/update_cmd.py:_print_stash_cleanup_guidance
 * Build the stash-cleanup guidance text. If stash_selector is non-NULL, emits
 * a "git stash drop <selector>" hint; otherwise emits a "look for commit <ref>"
 * hint. Returns a malloc'd string (caller frees). */
char *uc_stash_cleanup_guidance(const char *stash_ref,
                                const char *stash_selector);

/* PoP: _print_items @ hermes_cli/update_cmd.py:_print_items
 * Render a list of items (each either a dict-like struct or a bare name string)
 * under a label, mirroring the Python printer. items_json is a JSON array of
 * objects (each {"key":..., "description":...}) OR a JSON array of strings.
 * Returns a malloc'd string (caller frees) — the joined printable block. */
char *uc_print_items(const char *items_json, const char *label,
                     const char *key, const char *fallback_key);

/* PoP: _count_commits_between @ hermes_cli/update_cmd.py:_count_commits_between
 * Parse `git rev-list --count base..head` stdout into an int. Returns -1 on
 * parse failure (mirrors Python's -1 sentinel for "could not determine"). */
int uc_count_commits_between(const char *revlist_stdout);

/* --- Constants shared with the Python originals --- */
#define UC_SKIP_UPSTREAM_PROMPT_FILE ".skip_upstream_prompt"

/* OFFICIAL_REPO_URLS — the canonical NousResearch URLs that mean "not a fork". */
extern const char *const UC_OFFICIAL_REPO_URLS[];

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_CLI_UPDATE_CMD_H */
