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
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── hermes_cli/update_cmd.py pure helpers ────────────────────────────── */

/* PoP: _format_time_ago @ hermes_cli/update_cmd.py:_format_time_ago */
/* Render an ISO-8601 timestamp as "5m ago" / "3h ago" / "2d ago" / "just now".
 * Best-effort: returns "recently" on any parse failure. Caller does NOT free. */
const char *uc_format_time_ago(const char *iso_ts);

/* PoP: _stash_apply_failed_only_on_existing_untracked @ hermes_cli/update_cmd.py:_stash_apply_failed_only_on_existing_untracked */
/* True when a git stash apply failure stderr is ONLY about untracked files
 * that already exist in the working tree (the permission-denied autostash
 * tail-end). Any other error line => False. */
bool uc_stash_apply_failed_only_on_existing_untracked(const char *stderr_text);

/* PoP: _is_fork @ hermes_cli/update_cmd.py:_is_fork */
/* True when origin_url is NOT one of the official NousResearch URLs.
 * Normalizes by stripping a trailing "/" and trailing ".git" before compare. */
bool uc_is_fork_origin(const char *origin_url);

/* PoP: _service_restart_sec @ hermes_cli/update_cmd.py:_service_restart_sec */
/* Parse a systemd RestartUSec value ("30s", "100ms", "1min 30s", "infinity")
 * into seconds. Returns default on any parse miss or "infinity". */
double uc_parse_restart_sec(const char *raw, double default_sec);

/* PoP: _format_concurrent_instances_message @ hermes_cli/update_cmd.py:_format_concurrent_instances_message */
/* Build the human-readable blocker message for running hermes.exe instances.
 * Returns a malloc'd string (caller frees). matches is an array of "pid\tname"
 * tab-separated strings; n_matches is the count. scripts_dir is the path used
 * to locate hermes.exe. */
char *uc_format_concurrent_instances_message(const char **matches,
                                             size_t n_matches,
                                             const char *scripts_dir);

/* PoP: _resolve_stash_selector @ hermes_cli/update_cmd.py:_resolve_stash_selector */
/* Given "git stash list" output (one "selector commit" line per entry) and a
 * target commit SHA, return the matching stash selector (e.g. "stash@{1}")
 * or NULL if not found. Returns a malloc'd string (caller frees). */
char *uc_resolve_stash_selector(const char *stash_list_output,
                                const char *stash_ref);

/* PoP: _print_stash_cleanup_guidance @ hermes_cli/update_cmd.py:_print_stash_cleanup_guidance */
/* Build the stash-cleanup guidance text. If stash_selector is non-NULL, emits
 * a "git stash drop <selector>" hint; otherwise emits a "look for commit <ref>"
 * hint. Returns a malloc'd string (caller frees). */
char *uc_stash_cleanup_guidance(const char *stash_ref,
                                const char *stash_selector);

/* PoP: _print_items @ hermes_cli/update_cmd.py:_print_items */
/* Render a list of items (each either a dict-like struct or a bare name string)
 * under a label, mirroring the Python printer. items_json is a JSON array of
 * objects (each {"key":...,"description":...}) OR a JSON array of strings.
 * Returns a malloc'd string (caller frees) — the joined printable block. */
char *uc_print_items(const char *items_json, const char *label,
                     const char *key, const char *fallback_key);

/* PoP: _count_commits_between @ hermes_cli/update_cmd.py:_count_commits_between */
/* Parse "git rev-list --count base..head" stdout into an int. Returns -1 on
 * parse failure (mirrors Python's -1 sentinel for "could not determine"). */
int uc_count_commits_between(const char *revlist_stdout);

/* PoP: _is_android_python @ hermes_cli/update_cmd.py:_is_android_python */
/* True when running on an Android/Termux host (sys.platform == "android"). */
bool uc_is_android_python(void);

/* PoP: _npm_bin_exists @ hermes_cli/update_cmd.py:_npm_bin_exists */
/* True when an npm bin shim for *name* exists in *bin_dir* (POSIX or Windows).
 * Checks name, name.cmd, name.ps1, name.exe like the Python original. */
bool uc_npm_bin_exists(const char *bin_dir, const char *name);

/* PoP: _web_toolchain_roots @ hermes_cli/update_cmd.py:_web_toolchain_roots */
/* Roots whose node_modules/.bin can satisfy the web build.
 * Returns a malloc'd, NULL-terminated array of paths (caller frees with
 * uc_free_string_array). Always returns exactly [web_dir, web_dir.parent]. */
char **uc_web_toolchain_roots(const char *web_dir);

/* PoP: _web_build_toolchain_ready @ hermes_cli/update_cmd.py:_web_build_toolchain_ready */
/* True when tsc and vite shims are reachable from any of *roots* bin dirs. */
bool uc_web_build_toolchain_ready(const char **roots);

void uc_free_string_array(char **arr);

/* PoP: _format_venv_python_holders_message @ hermes_cli/update_cmd.py:_format_venv_python_holders_message */
/* Build the "venv processes are running" blocker message.
 * matches is an array of "pid\tname\tcmdline" tab-separated strings.
 * Returns a malloc'd string (caller frees). */
char *uc_format_venv_python_holders_message(const char **matches, size_t n_matches);

/* PoP: _resolve_pre_update_backup_mode @ hermes_cli/update_cmd.py:_resolve_pre_update_backup_mode */
/* Resolve backup mode from CLI flags + raw config value.
 * no_backup / backup are the CLI flag bools. raw_config is the raw value
 * from updates.pre_update_backup (can be bool True/False or a string).
 * raw_config_json is "true"/"false" for the legacy boolean form, or a
 * mode string like "quick"/"off"/"full". Set raw_config_json to "" when
 * the key is absent. Returns one of "off", "quick", "full". */
const char *uc_resolve_pre_update_backup_mode(bool no_backup, bool backup,
                                               const char *raw_config_json);

/* PoP: _parse_numstat_paths @ hermes_cli/update_cmd.py:_real_dirty */
/* Parse git diff --numstat output (one "<added>\t<deleted>\t<path>" per line)
 * into a set of paths. Returns a malloc'd, NULL-terminated array of unique
 * paths (caller frees with uc_free_string_array). */
char **uc_parse_numstat_paths(const char *numstat_output);

/* PoP: _get_origin_url @ hermes_cli/update_cmd.py:_get_origin_url */
/* Get the URL of the origin remote, or NULL if not set. */
const char *uc_get_origin_url(const char *git_cmd[], const char *cwd);

/* PoP: _has_upstream_remote @ hermes_cli/update_cmd.py:_has_upstream_remote */
/* True when an 'upstream' remote already exists. */
bool uc_has_upstream_remote(const char *git_cmd[], const char *cwd);

/* PoP: _add_upstream_remote @ hermes_cli/update_cmd.py:_add_upstream_remote */
/* Add the official repo as the 'upstream' remote. Returns true on success. */
bool uc_add_upstream_remote(const char *git_cmd[], const char *cwd);

/* PoP: _should_skip_upstream_prompt @ hermes_cli/update_cmd.py:_should_skip_upstream_prompt */
/* True when the user previously declined to add upstream. */
bool uc_should_skip_upstream_prompt(const char *hermes_home);

/* PoP: _mark_skip_upstream_prompt @ hermes_cli/update_cmd.py:_mark_skip_upstream_prompt */
/* Create marker file to skip future upstream prompts. */
void uc_mark_skip_upstream_prompt(const char *hermes_home);

/* PoP: _sync_fork_with_upstream @ hermes_cli/update_cmd.py:_sync_fork_with_upstream */
/* Attempt to push updated main to origin (sync fork). Returns true on success. */
bool uc_sync_fork_with_upstream(const char *git_cmd[], const char *cwd);

/* PoP: _npm_manifest_paths @ hermes_cli/update_cmd.py:_npm_manifest_paths */
/* Manifests whose changes must defeat the update-skip: package-lock.json,
 * root package.json, plus every workspace package.json pulled from the root
 * package.json "workspaces" globs (legacy {"packages": [...]} form honored).
 * Falls back to just the lockfile + root package.json on any read/parse error.
 * Returns a malloc'd, NULL-terminated array of paths (caller frees with
 * uc_free_string_array). */
char **uc_npm_manifest_paths(const char *project_root);

/* PoP: _npm_manifests_digest @ hermes_cli/update_cmd.py:_npm_manifests_digest */
/* Combined sha256 hex digest over the relative path + bytes of every
 * manifest from _npm_manifest_paths. Returns NULL when package-lock.json is
 * missing (never skip then). Unreadable files hash as "<missing>".
 * Returns a malloc'd string (caller frees). */
char *uc_npm_manifests_digest(const char *project_root);

/* PoP: _npm_lockfile_changed @ hermes_cli/update_cmd.py:_npm_lockfile_changed */
/* True when the recorded npm manifest digest (keyed by PROJECT_ROOT) differs
 * from the current one, when node_modules is missing, or when the web build
 * toolchain never landed. Returns true on any cache/read error. */
bool uc_npm_lockfile_changed(const char *project_root, const char *hermes_root);

/* PoP: _record_npm_lockfile_hash @ hermes_cli/update_cmd.py:_record_npm_lockfile_hash */
/* Write the current npm manifest digest to the per-root cache file
 * (.npm_lock_hash_<key12> under hermes_root). Silent on any error. */
void uc_record_npm_lockfile_hash(const char *project_root, const char *hermes_root);

/* PoP: _write_marker_file @ hermes_cli/update_cmd.py:_write_marker_file */
/* Drop an update-recovery breadcrumb: "started=<unix-ts>\npid=<pid>\n".
 * Never raises (silent on write error). */
void uc_write_marker_file(const char *path);

/* PoP: _write_update_incomplete_marker @ hermes_cli/update_cmd.py:_write_update_incomplete_marker */
/* Drop the interrupted core-install breadcrumb at project_root/.update-incomplete.
 * Never raises. */
void uc_write_update_incomplete_marker(const char *project_root);

/* PoP: _write_lazy_refresh_incomplete_marker @ hermes_cli/update_cmd.py:_write_lazy_refresh_incomplete_marker */
/* Drop the interrupted lazy-refresh breadcrumb at
 * project_root/.lazy-refresh-incomplete. Never raises. */
void uc_write_lazy_refresh_incomplete_marker(const char *project_root);

/* PoP: _finish_dashboard_update_cleanup @ hermes_cli/update_cmd.py:_finish_dashboard_update_cleanup */
/* Build the dashboard cleanup message. When node_failures is non-NULL and
 * non-empty, emits the "Leaving running dashboard process(es) untouched"
 * advisory. Otherwise emits the restart-required warning. Returns a malloc'd
 * string (caller frees), or NULL when there is nothing to print (no
 * unrecovered dashboard — the caller's _kill_stale_dashboard_processes result
 * is passed as unrecovered; pass false to suppress). */
char *uc_finish_dashboard_update_cleanup(const char **node_failures,
                                          size_t n_failures,
                                          bool unrecovered);

/* PoP: _for_each_systemd_gateway_unit @ hermes_cli/update_cmd.py:_for_each_systemd_gateway_unit */
/* Parse `systemctl list-units` stdout and return the service names of every
 * `hermes-gateway*.service` unit (suffix stripped, discovery order preserved).
 * Returns a malloc'd, NULL-terminated array (caller frees with
 * uc_free_string_array). */
char **uc_for_each_systemd_gateway_unit(const char *list_units_stdout);

/* PoP: _warn_incomplete_gateway_fleet_restart @ hermes_cli/update_cmd.py:_warn_incomplete_gateway_fleet_restart */
/* Build the incomplete-update warning for unrestarted gateway units.
 * failed_units is a NULL-terminated array; names are de-duplicated in
 * discovery order. Returns NULL when empty (nothing to warn), otherwise a
 * malloc'd string (caller frees). */
char *uc_warn_incomplete_gateway_fleet_restart(const char **failed_units);

/* PoP: _print_curator_first_run_notice @ hermes_cli/update_cmd.py:_print_curator_first_run_notice */
/* Build the skill-curator first-run notice block. curator_enabled controls
 * the gate; has_last_run_at indicates the curator has run before; interval_hours
 * is the curator interval (defaults to 168 = 24*7 when 0). Returns a malloc'd
 * string (caller frees), or NULL when there is nothing to print. */
char *uc_print_curator_first_run_notice(bool curator_enabled,
                                        bool has_last_run_at,
                                        int interval_hours);

/* PoP: _print_curator_recent_run_notice @ hermes_cli/update_cmd.py:_print_curator_recent_run_notice */
/* Build the curator recent-run summary notice. Printed only when the curator
 * has run (last_run_at non-NULL), the summary hasn't been shown for this run
 * (last_run_summary_shown_at != last_run_at), and the summary has renames
 * (contains a newline). when_is is the formatted "Xh ago"/"Xd ago" string from
 * uc_format_time_ago. summary is the raw curator summary text. Returns a
 * malloc'd string (caller frees), or NULL when there is nothing to print. */
char *uc_print_curator_recent_run_notice(const char *last_run_at,
                                         const char *shown_at,
                                         const char *summary,
                                         const char *when_is);

/* PoP: _discard_lockfile_churn @ hermes_cli/update_cmd.py:_discard_lockfile_churn */
/* Pure selection half of _discard_lockfile_churn: given `git diff --name-only`
 * stdout, return the package-lock.json paths to restore — every dirty lockfile
 * whose parent directory does NOT also have a dirty package.json. Returns a
 * malloc'd, NULL-terminated array (caller frees with uc_free_string_array). */
char **uc_select_lockfile_churn(const char *diff_stdout);

/* PoP: _restore_stashed_changes @ hermes_cli/update_cmd.py:_restore_stashed_changes */
/* Restore a stash created before an update. When prompt_user is true, asks
 * on stdin ("Restore local changes now? [Y/n]", default y) before applying.
 * Runs `git stash apply <ref>`, detects conflicts (git diff --diff-filter=U),
 * resets --hard on conflict (stash preserved), then resolves the stash
 * selector and drops the entry. Returns true when the stash was restored,
 * false when skipped/conflicted. git_cmd is the git argv prefix (first
 * element "git", may include "-c" flags); cwd is the repo root. */
bool uc_restore_stashed_changes(const char *git_cmd[], const char *cwd,
                                const char *stash_ref, bool prompt_user);

/* PoP: _discard_stashed_changes @ hermes_cli/update_cmd.py:_discard_stashed_changes */
/* Drop a stash created before an update without applying it (the
 * non-interactive "discard local changes" policy). Resolves the stash
 * selector then runs `git stash drop`. Returns true when dropped, false on
 * any git failure (stash left in place for safety). */
bool uc_discard_stashed_changes(const char *git_cmd[], const char *cwd,
                                const char *stash_ref);

/* PoP: _sync_with_upstream_if_needed @ hermes_cli/update_cmd.py:_sync_with_upstream_if_needed */
/* Fork upstream sync: adds the upstream remote on first run (prompted),
 * fetches upstream/main (--quiet), compares origin/main vs upstream/main,
 * fast-forwards origin from upstream when strictly behind, and pushes the
 * fork back to origin (--force-with-lease). Pure orchestration; all git
 * porcelain goes through web_git_run. hermes_home is used for the
 * .skip_upstream_prompt marker. */
void uc_sync_with_upstream_if_needed(const char *git_cmd[], const char *cwd,
                                     const char *hermes_home);

/* PoP: _discard_lockfile_churn @ hermes_cli/update_cmd.py:_discard_lockfile_churn */
/* Restore tracked package-lock.json files that npm dirtied locally.
 * Runs `git diff --name-only`, selects lockfiles whose parent has no dirty
 * package.json (via uc_select_lockfile_churn), then `git checkout --` them.
 * Best-effort; only ever touches package-lock.json paths. Returns the number
 * of files restored (0 when nothing to do). */
int uc_discard_lockfile_churn(const char *git_cmd[], const char *repo_root);

/* PoP: _normalize_managed_eol @ hermes_cli/update_cmd.py:_normalize_managed_eol */
/* Take a managed checkout off core.autocrlf=true without leaving it dirty.
 * Probes `git config --get core.autocrlf`; when not "true" it's a no-op.
 * Otherwise computes EOL-only churn (all-dirty minus real-dirty via numstat
 * with --ignore-cr-at-eol), checks those pathspecs back out under
 * `-c core.autocrlf=false`, and only then pins `core.autocrlf false`.
 * Best-effort; never blocks an update. Returns the number of normalized
 * files (0 when nothing to do or already pinned). */
int uc_normalize_managed_eol(const char *git_cmd[], const char *repo_root);

/* PoP: _run_logged_subprocess @ hermes_cli/update_cmd.py:_run_logged_subprocess */
/* Run a command with combined stdout+stderr captured (never shown on the
 * terminal) and appended to the given update.log FILE*. cmd is a NULL-
 * terminated argv (argv[0] is the program). Returns the process exit code
 * and, when out_combined is non-NULL, stores a malloc'd copy of the captured
 * output there (caller frees; "*out_combined = NULL" on spawn failure). */
int uc_run_logged_subprocess(char *const argv[], const char *cwd,
                             FILE *log_file, char **out_combined);

/* PoP: _cmd_update_check @ hermes_cli/update_cmd.py:_cmd_update_check */
/* Implement `hermes update --check`: fetch and report without installing.
 * Returns 0 = up to date, 1 = update available, -1 = error (fetch failed,
 * branch missing, not a repo). Mirrors the Python control flow: docker/nix
 * install methods are reported as errors; shallow repos compare tip SHAs. */
int uc_cmd_update_check(const char *project_root, const char *branch);

/* Delete the ".update_check" cache file under default_home and under every
 * profile directory (default_home/profiles/*). Best-effort, silent. */
void uc_invalidate_update_cache(const char *default_home);

/* PoP: _capture_head_sha @ hermes_cli/update_cmd.py:_capture_head_sha */
/* Parse `git rev-parse HEAD` stdout: strip surrounding whitespace, return the
 * SHA as a malloc'd string, or NULL if empty. */
char *uc_capture_head_sha(const char *rev_parse_stdout);

/* PoP: _stash_local_changes_if_needed @ hermes_cli/update_cmd.py:_stash_local_changes_if_needed */
/* Generate the autostash branch name for the given epoch seconds (UTC).
 * Format: "hermes-update-autostash-YYYYMMDD-HHMMSS". Returns a malloc'd
 * 32-byte string (caller frees). */
char *uc_autostash_name(time_t now_utc);

/* PoP: _ensure_fhs_path_guard @ hermes_cli/update_cmd.py:_ensure_fhs_path_guard */
/* Ensure /usr/local/bin is on PATH for RHEL-family root non-login shells.
 * Mirrors the post-symlink probe added to scripts/install.sh so that
 * existing FHS-layout root installs on RHEL/CentOS/Rocky/Alma 8+ get
 * repaired on hermes update without requiring a reinstall.
 * Silent no-op on: non-Linux, non-root, non-FHS installs, and any system
 * where bash -i -c 'command -v hermes' already resolves. Idempotent.
 * Returns true if it wrote a PATH guard, false otherwise. */
bool uc_ensure_fhs_path_guard(void);

/* PoP: _ensure_acp_launcher @ hermes_cli/update_cmd.py:_ensure_acp_launcher */
/* Self-heal: install a hermes-acp launcher next to the hermes one.
 * Mirrors the launcher block in scripts/install.sh so existing installs
 * gain the ACP command on hermes update without a reinstall.
 * No-op on Windows and wherever a hermes-acp already exists next to
 * hermes. Unwritable directories are skipped silently. Idempotent.
 * Returns true if it created the launcher, false otherwise. */
bool uc_ensure_acp_launcher(void);

/* PoP: _update_node_dependencies @ hermes_cli/update_cmd.py:_update_node_dependencies */
/* Refresh Node deps in the repo root and update workspaces.
 * Returns a malloc'd, NULL-terminated array of labels whose npm install
 * failed (empty array on success). The caller frees with uc_free_string_array.
 * Mirrors the Python logic: root-only install first, then ui-tui + web
 * workspaces (desktop deps are skipped — installed on demand). */
char **uc_update_node_dependencies(const char *project_root);

/* PoP: _upgrade_pip_before_lazy_refresh @ hermes_cli/update_cmd.py:_upgrade_pip_before_lazy_refresh */
/* Upgrade pip inside the managed venv before lazy-backend refreshes.
 * Older pip (e.g. 24.0 on Python 3.11) can fail setuptools-backed source
 * builds and leave a partially-written venv (#57828). Never raises.
 * install_cmd_prefix is the pip/uv command prefix (e.g. ["uv", "pip"]
 * or ["pip"]). env is an optional environment override (may be NULL). */
void uc_upgrade_pip_before_lazy_refresh(const char *install_cmd_prefix[],
                                        const char *env_path);

/* PoP: _refresh_active_lazy_features @ hermes_cli/update_cmd.py:_refresh_active_lazy_features */
/* Refresh lazy-installed backends after a code update.
 * Asks lazy_deps which features the user has previously activated and
 * reinstalls them under the current pins. Features the user never
 * enabled stay quiet. Returns a malloc'd string with the result summary
 * (caller frees), or NULL on total failure. install_cmd_prefix and env
 * are passed through to the repair probe on failure. */
char *uc_refresh_active_lazy_features(const char *install_cmd_prefix[],
                                      const char *env_path);

/* PoP: _refresh_active_memory_provider_dependencies @ hermes_cli/update_cmd.py:_refresh_active_memory_provider_dependencies */
/* Refresh pip dependencies for the configured external memory provider.
 * Re-runs the provider's declared install for the ACTIVE provider only,
 * after the core install and lazy refresh. Never raises.
 * Returns a malloc'd status string ("ok", "skipped", or "failed: <reason>"),
 * or NULL on config-load failure (caller frees). */
char *uc_refresh_active_memory_provider_dependencies(void);

/* PoP: _run_pre_update_backup @ hermes_cli/update_cmd.py:_run_pre_update_backup */
/* Run the pre-update safety backup and return the quick-snapshot id.
 * mode is "off", "quick", or "full". When mode is "off", returns NULL.
 * When mode is "quick", takes a state snapshot of critical small files
 * under state-snapshots/. When mode is "full", also creates a full zip
 * of HERMES_HOME under backups/. Returns a malloc'd snapshot id string
 * (caller frees), or NULL on failure. */
char *uc_run_pre_update_backup(const char *mode, const char *project_root);

/* PoP: _ensure_uv_for_termux @ hermes_cli/update_cmd.py:_ensure_uv_for_termux */
/* Best-effort uv bootstrap on Termux for faster update installs.
 * Prefers a uv already on PATH; only if there is none, falls back to a
 * wheel-only pip install uv so we never source-build the Rust crate.
 * pip_cmd is the pip/uv command prefix (e.g. {"uv","pip"}).
 * project_root is used as cwd for the pip install.
 * Returns a malloc'd path to the resolved uv binary (caller frees), or
 * NULL when no uv is available. */
char *uc_ensure_uv_for_termux(const char *pip_cmd[], const char *project_root);

/* PoP: _install_psutil_android_compat @ hermes_cli/update_cmd.py:_install_psutil_android_compat */
/* Install psutil on Android by downloading the upstream source, patching
 * platform detection (Termux reports sys.platform == 'android' which psutil's
 * setup gates on startswith('linux')), and installing with --no-build-isolation.
 * install_cmd_prefix is the pip/uv command prefix. The patch is applied only
 * to the extracted build tree in a temp directory — nothing is persisted.
 * Best-effort; never blocks an update. Returns 0 on success, -1 on failure. */
int uc_install_psutil_android_compat(const char *install_cmd_prefix[]);

/* PoP: _refresh_windows_gateway_launchers @ hermes_cli/update_cmd.py:_refresh_windows_gateway_launchers */
/* Regenerate installed Windows gateway launcher scripts after update.
 * The Scheduled Task / Startup-folder launchers (gateway.cmd + gateway.vbs)
 * are persistence artifacts written once at install time; this rewrites
 * them in place so legacy pythonw-era installs get the current
 * hidden-console design. No-op on non-Windows. Best-effort. */
void uc_refresh_windows_gateway_launchers(void);

/* PoP: _stage_replacement @ hermes_cli/update_cmd.py:_stage_replacement */
/* Copy src (dir or file) to dst's sibling staging path "<dst>.hermes-update-staging".
 * Restores dst from "<dst>.hermes-update-old" if dst is missing but the backup
 * exists (a previous run died mid-swap). Clears stale staging/backup leftovers.
 * Returns a malloc'd staging path (caller frees), or NULL on failure. */
char *uc_stage_replacement(const char *src, const char *dst);

/* PoP: _discard_staged @ hermes_cli/update_cmd.py:_discard_staged */
/* Remove staging paths (never committed). staged_pairs is a NULL-terminated
 * array of "staging\tdst" tab-separated strings. Best-effort, silent. */
void uc_discard_staged(const char **staged_pairs);

/* PoP: _commit_staged_replacements @ hermes_cli/update_cmd.py:_commit_staged_replacements */
/* Swap every staged entry into place (rename each live dst aside, rename
 * staging in), rolling all back on any failure so the tree lands wholly new
 * or wholly old. On success drops the backups. staged_pairs is a
 * NULL-terminated array of "staging\tdst" tab-separated strings. Returns 0
 * on full success, -1 if a swap failed (all entries rolled back). */
int uc_commit_staged_replacements(const char **staged_pairs);

/* PoP: _atomic_replace_dir @ hermes_cli/update_cmd.py:_atomic_replace_dir */
/* Replace directory dst with src without leaving dst half-deleted. Thin
 * alias over uc_stage_replacement + uc_commit_staged_replacements. Returns 0
 * on success, -1 on failure (tree rolled back). */
int uc_atomic_replace_dir(const char *src, const char *dst);

/* PoP: _gateway_prompt @ hermes_cli/update_cmd.py:_gateway_prompt */
/* File-based IPC prompt for gateway mode. Writes a JSON marker
 * at default_home/.update_prompt.json and polls for a response
 * at default_home/.update_response. Returns a malloc'd answer
 * string (caller frees), or NULL on error/timeout. */
char *uc_gateway_prompt(const char *default_home, const char *prompt_text,
                               const char *default_answer, double timeout_sec);

/* Generate a UUID v4 string (37 bytes including NUL). Returns
 * a malloc'd string (caller frees), or NULL on failure. */
char *uc_generate_uuid(void);

/* PoP: _wait_for_service_active @ hermes_cli/update_cmd.py:_wait_for_service_active */
/* Poll systemctl is-active for *service_name* every poll_ms
 * milliseconds until timeout_sec elapses. Returns 1 if active,
 * 0 if timed out, -1 on error. */
int uc_wait_for_service_active(const char *service_name,
                                     double timeout_sec,
                                     int poll_ms);

/* PoP: _log_only_write @ hermes_cli/update_cmd.py:_log_only_write */
/* Append text to update.log (the given FILE*), adding a trailing newline when
 * absent. Never flushes the terminal; returns the number of bytes written or
 * -1 on error. */
long uc_log_only_write(FILE *log_file, const char *text);

/* PoP: _write_update_planned_stop_marker @ hermes_cli/update_cmd.py:_write_update_planned_stop_marker */
/* Write a planned-stop marker JSON into profile_path/.gateway-planned-stop.json
 * with compact separators (",", ":"). Returns 0 on success, -1 on error. */
int uc_write_update_planned_stop_marker(const char *profile_path, long pid,
                                        long stopper_pid,
                                        const char *target_start_time,
                                        const char *written_at);

/* PoP: _print_fts_optimize_available_notice @ hermes_cli/update_cmd.py:_print_fts_optimize_available_notice */
/* Build the FTS optimize advisory/required notice. mode is "advise", "require"
 * or "off" (empty defaults to "advise"). size_gb is the state.db size in GB;
 * only notices when >= 0.5. interrupted indicates an incomplete prior run.
 * Returns a malloc'd string (caller frees), or NULL when nothing to print. */
char *uc_print_fts_optimize_available_notice(const char *mode, double size_gb,
                                             bool interrupted);

/* --- Constants shared with the Python originals --- */
#define UC_SKIP_UPSTREAM_PROMPT_FILE ".skip_upstream_prompt"

/* OFFICIAL_REPO_URLS — the canonical NousResearch URLs that mean "not a fork". */
extern const char *const UC_OFFICIAL_REPO_URLS[];

/* PoP: _update_via_zip @ hermes_cli/update_cmd.py:_update_via_zip */
/* Update Hermes by downloading a ZIP archive from GitHub.
 * Windows fallback used when git file I/O is broken.
 * branch must be "main" (ZIP path does not support other branches).
 * project_root is the install directory. Returns 0 on success, -1 on
 * failure (exit code 1 matches Python's sys.exit(1) on ZIP failure). */
int uc_update_via_zip(const char *branch, const char *project_root);

/* PoP: _venv_core_imports_healthy @ hermes_cli/update_cmd.py:_venv_core_imports_healthy */
/* Probe the project venv for core imports needed to boot.
 * Mirrors the Python import probe: runs <venv_python> -c "import ..."
 * and checks return code + stdout for missing modules.
 * project_root is the install root (expects a venv/ subdir).
 * is_windows controls which interpreter path is used.
 * Sets *healthy to true if all core modules import cleanly; the detail
 * string (malloc'd, caller frees) describes what's missing when unhealthy.
 * Returns 0 on success, -1 on probe failure. */
int uc_venv_core_imports_healthy(const char *project_root, bool is_windows,
                                   bool *healthy, char **detail);

/* PoP: _detect_venv_python_processes @ hermes_cli/update_cmd.py:_detect_venv_python_processes */
/* Find live processes running from the project venv's interpreter.
 * Returns a malloc'd, NULL-terminated array of "pid\tname\tcmdline"
 * strings (caller frees with uc_free_string_array). Returns NULL on
 * non-Windows or when psutil is unavailable. */
char **uc_detect_venv_python_processes(const char *project_root);

/* PoP: _venv_launcher_ancestors @ hermes_cli/update_cmd.py:_venv_launcher_ancestors */
/* Return venv-interpreter ancestors of *pids* that hold the install open.
 * pids is a NULL-terminated array of stringified PIDs. project_root is
 * the install root. Returns a malloc'd, NULL-terminated array of
 * stringified PIDs (caller frees with uc_free_string_array).
 * No-op on non-Windows. */
char **uc_venv_launcher_ancestors(const char **pids, const char *project_root);

/* PoP: _leftover_pausable_gateway_pids @ hermes_cli/update_cmd.py:_leftover_pausable_gateway_pids */
/* Filter *matches* to only pausable gateway processes.
 * matches is a NULL-terminated array of "pid\tname\tcmdline" strings.
 * Returns a malloc'd, NULL-terminated array of stringified PIDs
 * (caller frees with uc_free_string_array), or NULL when any holder is
 * NOT a pausable gateway (signalling the caller must refuse). */
char **uc_leftover_pausable_gateway_pids(const char **matches);

/* PoP: _pause_windows_gateways_for_update @ hermes_cli/update_cmd.py:_pause_windows_gateways_for_update */
/* Stop running Windows gateways before mutating the checkout or venv.
 * Returns a malloc'd JSON string with resume info (caller frees), or
 * NULL when on non-Windows / nothing running. */
char *uc_pause_windows_gateways_for_update(const char *project_root);

/* PoP: _cold_start_windows_gateway_after_update @ hermes_cli/update_cmd.py:_cold_start_windows_gateway_after_update */
/* Start a fresh detached gateway after update when one is installed but down.
 * No-op on non-Windows. Best-effort and idempotent. */
void uc_cold_start_windows_gateway_after_update(const char *project_root);

/* PoP: _resume_windows_gateways_after_update @ hermes_cli/update_cmd.py:_resume_windows_gateways_after_update */
/* Restart Windows profile gateways previously paused for update.
 * resume_json is the JSON token returned by uc_pause_windows_gateways_for_update.
 * project_root is the install root. No-op on non-Windows. */
void uc_resume_windows_gateways_after_update(const char *resume_json,
                                             const char *project_root);

/* PoP: _wait_for_windows_update_gateway_exit @ hermes_cli/update_cmd.py:_wait_for_windows_update_gateway_exit */
/* Wait for the given gateway PIDs to exit, returning survivors.
 * pids is a NULL-terminated array of stringified PIDs. timeout is the
 * max seconds to wait. Returns a malloc'd, NULL-terminated array of
 * survivor PIDs as strings (caller frees with uc_free_string_array). */
char **uc_wait_for_windows_update_gateway_exit(const char **pids, double timeout);

/* PoP: _cmd_update_impl @ hermes_cli/update_cmd.py:_cmd_update_impl */
/* Body of cmd_update. Mirrors the Python _cmd_update_impl orchestration:
 * backup, pause gateways, detect VENV holders, git fetch/pull (or ZIP
 * fallback), resolve stash, commit, dependency refresh, etc.
 * project_root is the install root. assume_yes skips interactive prompts.
 * force_venv bypasses the venv-holder guard. Returns 0 on success,
 * non-zero on failure (matching Python exit codes). */
int uc_cmd_update_impl(const char *project_root, bool assume_yes,
                       bool force_venv, bool gateway_mode);

/* PoP: _reload_updated_runtime_modules @ hermes_cli/update_cmd.py:_reload_updated_runtime_modules */
/* Refresh update-sensitive runtime state after the checkout changes in-place.
 * The Python original uses importlib.reload() on hermes_constants,
 * tools.environments.local and tools.lazy_deps so the still-running updater
 * sees new symbols. In the C port the same three sources are consulted
 * through env/cache reads, so the faithful equivalent is invalidating any
 * cached copies (re-read them next use). Never raises; best-effort. */
void uc_reload_updated_runtime_modules(void);

/* PoP: _validate_critical_files_syntax @ hermes_cli/update_cmd.py:_validate_critical_files_syntax */
/* Compile-check the critical first-party Python files after a pull so a
 * bad merge (orphan conflict markers, renamed symbol) auto-rolls-back
 * instead of bricking the CLI. Mirrors py_compile by shelling out to a
 * Python interpreter with `-m py_compile` on each file. Files that don't
 * exist are skipped (a future refactor may remove them).
 * Returns 0 when all present files compile, 1 on syntax error
 * (*failing_path* and *error_message* are malloc'd, caller frees), or
 * -1 when no Python interpreter is available (cannot validate — callers
 * treat that as pass). */
int uc_validate_critical_files_syntax(const char *root,
                                      char **failing_path,
                                      char **error_message);

/* PoP: _validate_critical_modules_import @ hermes_cli/update_cmd.py:_validate_critical_modules_import */
/* Import the critical first-party modules in a subprocess to catch
 * cross-module breakage (syntax-valid but import-broken combination).
 * Uses the project venv's interpreter when present, else the running
 * python. Only our own packages count as breakage — a missing third-party
 * dep means dependencies aren't installed yet, not a skewed checkout.
 * Returns 0 on healthy import, 1 on import failure (*failing_module* and
 * *error_message* malloc'd, caller frees), -1 when no interpreter or the
 * probe couldn't run (callers treat as pass). */
int uc_validate_critical_modules_import(const char *root,
                                        char **failing_module,
                                        char **error_message);

/* PoP: _m @ hermes_cli/update_cmd.py:_m */
/* Lazy hermes_cli.main reference. Python: from hermes_cli import main;
 * return main — lets callers patch hermes_cli.main.<helper> and defers the
 * import so hermes_cli.main -> hermes_cli.update_cmd stays one-way at
 * import time. In C there is no module table; the faithful equivalent is
 * resolving the main module's runtime surface (the install root) on each
 * call so the port's "module reference" never goes stale after an update.
 * Returns a malloc'd install-root string (caller frees), or NULL when
 * unresolvable. */
char *uc_m(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_CLI_UPDATE_CMD_H */
