/*
 * noc_w30_base_0c99d4_p2.h - C surface for the command-construction core of
 * upstream tools/environments/base.py (gate w30_base_0c99d4_p2).
 *
 * Ported: the cd-target and shell-path quoting, the payload-mode stdin
 * redirect and heredoc embedding, the staged stdin path, the cwd-marker
 * split/parse pair, the full per-command bash wrapper and the login-snapshot
 * bootstrap script.
 *
 * Seams (passed in, never touched here): the uuid token, the passthrough
 * allowlist, the delegated-task shell-state chain.
 */
#ifndef NOC_W30_BASE_0C99D4_P2_H
#define NOC_W30_BASE_0C99D4_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {cwd} -> the cd target, ~ preserved through $HOME. */
json_t *w30_base_0c99d4_quote_cwd_for_cd(const json_t *in);

/* in: {path} -> shlex.quote(path). */
json_t *w30_base_0c99d4_quote_shell_path(const json_t *in);

/* in: {command, path} -> "exec 0< q || exit $?\nrm -f -- q || exit $?\ncmd". */
json_t *w30_base_0c99d4_stdin_redirect(const json_t *in);

/* in: {command, stdin_data, token} -> the heredoc-wrapped command. */
json_t *w30_base_0c99d4_stdin_heredoc(const json_t *in);

/* in: {temp_dir, token} -> "<root>/.hermes-stdin-<token>". */
json_t *w30_base_0c99d4_staged_stdin_path(const json_t *in);

/* in: {output, marker} -> {path|null, cleaned} or null when no pair exists. */
json_t *w30_base_0c99d4_split_cwd_marker(const json_t *in);

/* in: {output, pipe_marker, cwd_marker} ->
 * {output, cwd_observed?, cwd?, pipeline_status|null}. */
json_t *w30_base_0c99d4_parse_cwd_marker(const json_t *in);

/* in: {command, cwd, snapshot_path, snapshot_ready, persist_env,
 *       pipe_marker, cwd_marker, cwd_fallback, cwd_fallback_marker?,
 *       passthrough_names?, chain?} -> the per-command bash script. */
json_t *w30_base_0c99d4_wrap_command(const json_t *in);

/* in: {cwd, snapshot_path, cwd_marker, excluded_names?} -> the snapshot
 * bootstrap script. */
json_t *w30_base_0c99d4_bootstrap_script(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
