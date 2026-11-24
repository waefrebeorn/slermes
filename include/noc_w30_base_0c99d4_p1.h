/*
 * noc_w30_base_0c99d4_p1.h - C surface for the env / capability /
 * derivation core of upstream tools/environments/base.py
 * (gate w30_base_0c99d4_p1).
 *
 * Ported: the sandbox-root derivation, the host-filesystem capability
 * predicate, the activity-callback throttle, the shell env-name validator and
 * the profile-scoped snapshot exclusion set, the sudo / non-login probe
 * coercions, the session path + marker derivations, the per-task snapshot
 * digest, cwd-marker parsing/publishing, the readlink-parse of fetch_realpath,
 * the cwd identity hook, the backend temp dir and the quoting kwargs shared by
 * the bootstrap and the per-command wrapper.
 *
 * Seams (passed in, never touched here): the host environ, the backend bash
 * process, the exec-channel result.
 */
#ifndef NOC_W30_BASE_0C99D4_P1_H
#define NOC_W30_BASE_0C99D4_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {env: {TERMINAL_SANDBOX_DIR?, HERMES_HOME?, LOCALAPPDATA?,
 *            HERMES_DATA_DIR_SUFFIX?}} -> the sandbox root, posix form. */
json_t *w30_base_0c99d4_sandbox_dir(const json_t *in);

/* in: {value, present?} -> bool; only a literal JSON true counts. */
json_t *w30_base_0c99d4_uses_host_filesystem(const json_t *in);

/* in: {last_touch, start, now, interval?, has_cb?, raise?, label?} ->
 * {fired, last_touch, message|null}. */
json_t *w30_base_0c99d4_activity_due(const json_t *in);

/* in: {name} -> bool (^[A-Za-z_][A-Za-z0-9_]*$). */
json_t *w30_base_0c99d4_env_name_valid(const json_t *in);

/* in: {profile_scoped, multiplex, names?, additional?} -> sorted name list. */
json_t *w30_base_0c99d4_snapshot_exclusions(const json_t *in);

/* in: {supported, probe_rc, probe_raises?} -> bool. */
json_t *w30_base_0c99d4_sudo_nopasswd(const json_t *in);

/* in: {detail, probe_rc, probe_stdout?, probe_raises?, probe_exc?} ->
 * [prefer_nonlogin, detail]. */
json_t *w30_base_0c99d4_probe_nonlogin(const json_t *in);

/* in: {temp_dir, session_id} -> {snapshot_path, cwd_file, cwd_marker,
 * pipe_marker, cwd_fallback_marker}. */
json_t *w30_base_0c99d4_init_paths(const json_t *in);

/* in: {session_id} -> {cwd, cwd_fallback, pipe}. */
json_t *w30_base_0c99d4_session_markers(const json_t *in);

/* in: {snapshot_path, task_id} -> "<snap stem>-<sha256[:16]>.sh". */
json_t *w30_base_0c99d4_task_snapshot_path(const json_t *in);

/* in: {output, pipe_marker, cwd_marker, move_session, env_cwd} ->
 * {output, cwd_observed?, cwd?, pipeline_status|null, session_cwd}. */
json_t *w30_base_0c99d4_update_cwd(const json_t *in);

/* in: {result: {returncode, output}} -> resolved path or null. */
json_t *w30_base_0c99d4_fetch_realpath(const json_t *in);

/* in: {cwd} -> {cwd, fallback: null}. */
json_t *w30_base_0c99d4_usable_cwd(const json_t *in);

/* in: {} -> "/tmp". */
json_t *w30_base_0c99d4_temp_dir(const json_t *in);

/* in: {cwd, snap?, snapshot_path, cwd_marker} -> {quoted_cwd, quoted_snap,
 * snap_tmp_template, cwd_marker}. */
json_t *w30_base_0c99d4_script_kwargs(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
