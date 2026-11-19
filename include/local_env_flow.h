/*
 * local_env_flow.h — local execution-environment FLOW cores of
 * tools/environments/local.py (PATH construction ordering, child-env resolution,
 * cwd fallback chains, process-group timeout/kill ladders, marker parsing,
 * error paths).
 *
 * Ports the DECISION sequences (call order, branch conditions, early returns,
 * exception -> fallback / suppress paths, mutation order) of the live module.
 * All live I/O (os.path probes, signals, process polls/waits/kills, psutil
 * snapshots, clock reads, shell discovery, policy reads, foreign env mutators)
 * enters through the universal seam below; the cores perform no I/O.
 *
 * Sequences are observable through a lef_trace_t: the harness wires the seam to
 * a recording stub that appends one JSON event per call (arguments + scripted
 * result) in call order; the oracle compares that ordered event list against
 * recording monkeypatch stubs driving the LIVE Python with the same scripts.
 *
 * ── Models ──────────────────────────────────────────────────────────────────
 * proc:        {"pid": <int>, "_hermes_pgid": <int|null>,
 *               "_hermes_pgid_start": <fingerprint|null>, ...}
 *              Absent keys model getattr(proc, name, None) == None. proc method
 *              calls (poll/wait/kill) are seams keyed by pid.
 * descendants: [{"pid": <int>}, ...] — the psutil children snapshot; each child's
 *              is_running()/kill() are seams keyed by pid.
 * env/result:  JSON objects mutated in place (Python dict semantics: json_set
 *              keeps an existing key's position, json_obj_del pops).
 * pgid:        a JSON number node, or NULL/JSON_NULL for Python None.
 *
 * ── Universal seam ──────────────────────────────────────────────────────────
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * name identifies the live call (table below); args is a BORROWED object of the
 * call's arguments; the result is a NEW json_t the caller frees. NULL == Python
 * None. A scripted exception is returned as {"__raise__": "<Kind>", "msg": ...};
 * lef_raise() detects it. Void seams return NULL. The harness records
 * {ev:name, args:<copy>, ret:<script>} per call; a scripted JSON null returns a
 * NULL pointer (NOT a json_null node) so truthiness tests read it as None.
 *
 * Script semantics (harness + oracle identical): io[name] is a single value
 * reused for every call, OR a bare array consumed one element per call (last
 * element repeats when exhausted). A constant LIST result must be wrapped as
 * {"__script__": true, "ret": [...]} so it is not read as a per-call queue.
 *
 * None-valued argument keys are OMITTED on both sides (drop_none), EXCEPT
 * bridged_env_digest's "value" (None is a meaningful digest input) and the
 * state-snapshot args of path_env_key / base_parse_cwd_marker /
 * apply_subprocess_home_env (always present).
 *
 * Effect seams: base_parse_cwd_marker and apply_subprocess_home_env model a
 * foreign in-place mutator; their scripted result is an updates object applied
 * to the target dict identically on both sides (NULL == no updates). The live
 * base mutators only SET keys (verified against base.py), so updates suffice.
 *
 * Import-failure flags (psutil_import_fail / status_import_fail) model a failed
 * `import psutil` / `from gateway.status import ...`: the import itself emits no
 * trace event on either side; only its branch effect is observable.
 *
 * Seam name -> live call (args keys; ret):
 *   dirname               os.path.dirname {path}; str
 *   basename              os.path.basename {path}; str
 *   path_join             os.path.join {parts:[...]}; str
 *   isdir                 os.path.isdir {path}; bool
 *   isabs                 os.path.isabs {path}; bool
 *   expanduser            os.path.expanduser {path}; str
 *   normcase              os.path.normcase {path}; str
 *   getpgid               os.getpgid {pid}; int (may raise)
 *   getpgrp               os.getpgrp {}; int (may raise)
 *   killpg                os.killpg {pgid,sig}; null (may raise)
 *   proc_poll             proc.poll {pid}; returncode|null (may raise)
 *   proc_wait             proc.wait {pid,timeout}; null (may raise)
 *   proc_kill             proc.kill {pid}; null (may raise)
 *   child_is_running      child.is_running {pid}; bool (may raise)
 *   child_kill            child.kill {pid}; null (may raise)
 *   psutil_children       psutil.Process(pid).children(recursive=True) {pid};
 *                         [{"pid":...},...] (may raise; null == empty snapshot)
 *   monotonic             time.monotonic {}; float
 *   sleep                 time.sleep {seconds}; null (may raise)
 *   get_process_start_time  gateway.status.get_process_start_time {pid};
 *                         fingerprint|null (may raise)
 *   fingerprints_match    gateway.status.start_time_fingerprints_match
 *                         {expected,current}; bool (may raise TypeError/ValueError)
 *   terminate_pid         gateway.status.terminate_pid {pid,force,
 *                         expected_start_time}; null (may raise)
 *   find_bash             local._find_bash {}; str (may raise)
 *   git_bash_bin_dirs     local._git_bash_bin_dirs {}; [str,...]
 *   user_facing_path      pm.user_facing.user_facing_path {path}; str
 *   path_env_key          local._path_env_key {env}; str|null
 *   msys_to_windows_path  local._msys_to_windows_path {cwd}; str
 *   resolve_safe_cwd      local._resolve_safe_cwd {cwd}; str
 *   kill_process          LocalEnvironment._kill_process {pid}; null (may raise)
 *   base_parse_cwd_marker BaseEnvironment._parse_cwd_marker {result};
 *                         updates object|null (effect applied to result)
 *   bridged_env_digest    gateway.bridged_env.bridged_env_digest {value};
 *                         str|null (value key kept even when null)
 *   secret_source_bootstrap_env  local_env_policy._secret_source_bootstrap_env
 *                         {}; [str,...]
 *   get_hermes_home_override  hermes_constants.get_hermes_home_override {};
 *                         str|null (may raise)
 *   apply_subprocess_home_env hermes_constants.apply_subprocess_home_env {env};
 *                         updates object|null (effect applied to env)
 *
 * String-valued seams are scripted with strings; env keys / path strings in
 * fixtures are ASCII (str.upper() / normcase modeling is ASCII). shlex.quote is
 * transcribed internally (pure stdlib leaf, no seam). signal.SIGTERM=15 /
 * SIGKILL=9 are POSIX constants (the live signal module on a Windows oracle
 * host lacks SIGKILL; the oracle seeds the POSIX values).
 *
 * JSON values are libjson json_t. Ownership is documented per function.
 * Python truthiness / or-chain / short-circuit order is preserved exactly.
 */

#ifndef LOCAL_ENV_FLOW_H
#define LOCAL_ENV_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Trace recorder (harness infrastructure, no Python counterpart) ─────────
 * events is a JSON_ARRAY of event objects. lef_trace_add takes ownership of ev
 * (frees it when t is NULL). The trace never leaves the caller. */
typedef struct lef_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} lef_trace_t;

void lef_trace_add(lef_trace_t *t, json_t *ev);
void lef_trace_free(lef_trace_t *t);

/* ── Universal seam ── */
typedef struct lef_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} lef_seams_t;

/* Raise detection: returns the kind string when r is a {"__raise__":...} spec,
 * else NULL. lef_raise_msg returns the scripted message ("" when absent). */
const char *lef_raise(const json_t *r);
const char *lef_raise_msg(const json_t *r);

/* ── PATH construction ──────────────────────────────────────────────────────
 * prepend_missing: pure leaf; returns malloc'd str (caller frees). dirs is a
 *   borrowed JSON array of str. pathsep is os.pathsep (":" / ";").
 * append_dropped: returns malloc'd str; *exc on a propagated seam exception.
 * compute_git_bash_bin_dirs: returns a NEW JSON array of str (caller frees).
 * user_tools_first: returns malloc'd str. */
char *lef_prepend_missing_path_entries(const char *existing_path,
                                       const json_t *dirs, const char *pathsep);
char *lef_append_dropped_path_entries(const char *cmd_string,
                                      const char *parent_path, bool is_windows,
                                      const lef_seams_t *s, json_t **exc);
json_t *lef_compute_git_bash_bin_dirs(const lef_seams_t *s, json_t **exc);
char *lef_user_tools_first(const char *existing_path, const char *pathsep,
                           const lef_seams_t *s, json_t **exc);

/* ── Child-env resolution ───────────────────────────────────────────────────
 * terminal_env: mutates env in place and RETURNS env (same pointer) on success,
 *   NULL when *exc is set. path_env_key is a seam; _user_tools_first runs for
 *   real (its seams fire).
 * apply_profile_home / carry_bridge_markers / restore_bootstrap_tokens: mutate
 *   env in place; ret is None. */
json_t *lef_terminal_env(json_t *env, const char *pathsep,
                         const lef_seams_t *s, json_t **exc);
void lef_apply_profile_home(json_t *env, const lef_seams_t *s, json_t **exc);
void lef_carry_bridge_markers(json_t *env, const json_t *source,
                              bool own_profile, const lef_seams_t *s,
                              json_t **exc);
void lef_restore_bootstrap_tokens(json_t *env, const json_t *source,
                                  const lef_seams_t *s, json_t **exc);

/* ── cwd resolution / marker parsing ────────────────────────────────────────
 * usable_cwd: returns a NEW 2-element array [cwd_str, fallback_obj|null]
 *   (projection of the live (str, dict|None) tuple); NULL when *exc is set.
 * parse_cwd_marker: mutates result in place; ret is None. */
json_t *lef_usable_cwd(const char *cwd, const char *self_cwd,
                       const lef_seams_t *s, json_t **exc);
void lef_parse_cwd_marker(json_t *result, const lef_seams_t *s, json_t **exc);

/* ── Process-group timeout / kill ladders (POSIX) + Windows kill ────────────
 * leader_is_ours / wait_for_group_exit: return bool; on a propagated exception
 *   *exc is set and the return is false. pgid is a borrowed number node or
 *   NULL/JSON_NULL (None). proc is a borrowed proc model.
 * sweep / kill_known_pids / kill_process_group_posix / kill_process_windows /
 *   force_kill_process: ret is None; descendants is a borrowed array of child
 *   models. kill_process_group_posix composes the REAL ports of _leader_is_ours,
 *   _wait_for_group_exit, _sweep_escaped_descendants, _kill_known_pids (their
 *   seams fire); force_kill_process composes the REAL _leader_is_ours port and
 *   models the claimed self._kill_process as the kill_process seam. */
bool lef_leader_is_ours(const json_t *pgid, const json_t *expected_start,
                        const lef_seams_t *s, json_t **exc);
bool lef_wait_for_group_exit(const json_t *proc, const json_t *pgid,
                             double timeout, const lef_seams_t *s,
                             json_t **exc);
void lef_sweep_escaped_descendants(const json_t *descendants,
                                   const json_t *pgid, const lef_seams_t *s,
                                   json_t **exc);
void lef_kill_known_pids(const json_t *proc, const json_t *descendants,
                         const lef_seams_t *s, json_t **exc);
void lef_kill_process_group_posix(const json_t *proc, bool psutil_import_fail,
                                  const lef_seams_t *s, json_t **exc);
void lef_kill_process_windows(const json_t *proc, bool status_import_fail,
                              const lef_seams_t *s, json_t **exc);
void lef_force_kill_process(const json_t *proc, bool is_windows,
                            const lef_seams_t *s, json_t **exc);

#ifdef __cplusplus
}
#endif

#endif /* LOCAL_ENV_FLOW_H */