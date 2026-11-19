/* port_sessions_cmd_flow.h — hermes_cli/sessions_cmd.py session-management FLOW.
 *
 * Decision-sequence port of the `hermes sessions` query/filter, export and
 * delete/pin/rename flows: the confirmation exit-code chain, the dry-run
 * preview / pinned-skip formatting gates, the flat-export renderer dispatch,
 * the output-path resolution branches and the delete error paths. Every
 * store/file/stdin dependency is an INJECTED seam: one universal callback
 * driven by scripted results, recording an ordered trace.
 *
 * Universal seam contract (shared with tests/oracle_sessions_cmd_flow.py):
 * the port calls scf_call(ctx, name, args); the harness supplies the
 * recording stub. args ownership is TRANSFERRED (NULL -> empty object);
 * the result is a NEW json_t (NULL == None).
 *
 * Seam names -> live call (None-valued arg keys are omitted on BOTH sides):
 *   print                          builtins.print(text, end=..., flush=...)
 *                                  args {text, end} (+flush:true when set);
 *                                  file is always stdout (omitted)
 *   confirm_prompt                 sessions_cmd._confirm_prompt(prompt)
 *                                  (PoP-claimed sibling; scripted bool/None)
 *   get_hermes_home                sessions_cmd.get_hermes_home() -> Path
 *                                  (script: POSIX home string; oracle wraps
 *                                  in a FakePath double, C joins lexically.
 *                                  The trace ret is the RAW script string;
 *                                  FakePath normalization surfaces only in
 *                                  the sessions_dir join.)
 *   native_inputs_root             gateway.session_ingress_media.native_inputs_root()
 *   sessions_delete                store.sessions.delete(session_id,
 *                                  refuse_guarded=True, media_root=..., **fences)
 *   after_sessions_removed         session_retention.after_sessions_removed(
 *                                  store, profile_dir, removed, reason=...,
 *                                  primary=..., sessions_dir=...)
 *                                  args {profile_dir, removed, reason, primary,
 *                                  sessions_dir}; the store handle itself is
 *                                  not serialized (profile_dir projects it)
 *   resolve_id                     store.sessions.resolve_id(session_id)
 *   session_get                    store.sessions.get(session_id) -> record
 *                                  double (script dict -> attribute object)
 *   set_title                      store.sessions.set_title(session_id, title)
 *   set_pinned                     store.sessions.set_pinned(session_id, pinned)
 *   get_title                      store.sessions.get_title(session_id)
 *   describe_filters               hermes_cli.session_filters.describe_filters(filters)
 *   print_truncated                hermes_cli.cli_output.print_truncated(more, hint="")
 *   count_prune_matches            store.maintenance.count_prune_matches(
 *                                  **bounds, criteria=criteria, pinned_only=True)
 *                                  args {**bounds, criteria: <25-field dict>,
 *                                  pinned_only: true}
 *   isdir                          os.path.isdir(path)
 *   makedirs                       os.makedirs(path, exist_ok=True)
 *   stdout_write                   sys.stdout.write(text)
 *   file_write                     open(path,"w",encoding="utf-8") + f.write(text)
 *                                  modeled as ONE effect seam fired at write()
 *   render_sessions_export         hermes_cli.session_export.render_sessions_export(
 *                                  sessions, fmt=..., only=...)
 *   export_record_count            hermes_cli.session_export.export_record_count(
 *                                  sessions, only=...) -> (count, noun)
 *   generate_html_export           hermes_cli.session_export_html.generate_html_export(session)
 *   generate_multi_session_html_export  ...generate_multi_session_html_export(sessions)
 *   default_save_filename          hermes_cli.session_export.default_save_filename(id, fmt)
 *   render_jsonl                   sessions_cmd._render_jsonl(args, sessions)
 *                                  (PoP-claimed sibling; args {args: vars(args)
 *                                  projection, sessions}; script [lines, summary])
 *   collect                        _export_flat's collect() callback (no args)
 *
 * Scripted results (case "io" object, seam name -> script):
 *   - absent key            -> single None result
 *   - {"$seq": [v1, v2...]} -> one element consumed per call; exhaustion raises
 *                              RuntimeError("script exhausted: <name>")
 *   - {"__raise__": cls, "msg": m, ...} -> raises that exception class; the
 *     trace "ret" is the WHOLE marker object. SessionWriteGuarded markers carry
 *     "session_id"/"msg" (msg absent -> the live default lease text);
 *     AuthorityRefusal markers carry "reason" (str(exc) == reason).
 *   - any other JSON value  -> reused for every call (NULL == None)
 *
 * Canonical value representation (both sides):
 *   - tuple/set/frozenset -> JSON array (sets sorted); PruneCriteria -> the
 *     full 25-field dict in declaration order (defaults filled: 22 nulls +
 *     include_pinned/lineage_tips_only/include_open false).
 *   - record/candidate doubles -> plain attribute dicts (vars); scripts carry
 *     every field the exercised path reads.
 *   - Paths are POSIX strings: the oracle runs os.path == posixpath (os.sep
 *     "/") and FakePath doubles whose str() is the posix string; pathlib
 *     joining collapses "." / duplicate slashes and keeps "..".
 *   - None -> JSON null (C: NULL pointer or JSON_NULL node, both read None).
 *   - uncaught exception -> {"__raise__": "<ClassName>"} (class only).
 *   - Integral floats normalize to ints on both sides; f-string number
 *     operands are integral in fixtures. str.strip() operand fields are ASCII
 *     strings/null (Unicode whitespace is out of the fixture domain).
 *   - int() operands (count seams / recovery counts) are numbers, bools or
 *     null in fixtures; null raises TypeError like live int(None).
 *   - f-string interpolation of None renders "None"; bools "True"/"False".
 *
 * State double: _export_flat mutates args.output in place; the harness emits
 * the post-call vars(args) window as out."state" ({} for every other func,
 * and the partial window even when the call raised).
 *
 * Internal unclaimed steps (_sessions_dir, _media_root) are transcribed as
 * INTERNAL helpers (no PoP claim) and stay REAL in the oracle via AST
 * extraction. _FLAT_EXPORTERS' unusable-gate lambdas are transcribed as
 * plain conditionals; the table is re-bound per case in the oracle so the
 * seamed _render_jsonl recorder is the renderer the live table dispatches to.
 */
#ifndef PORT_SESSIONS_CMD_FLOW_H
#define PORT_SESSIONS_CMD_FLOW_H

#include "hermes_json.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct scf_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted tables (profile_dir) */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} scf_ctx;

/* Core seam plumbing (harness wires ctx->io; port code only calls scf_call). */
json_t *scf_call(scf_ctx *ctx, const char *name, json_t *args);
bool scf_exc_pending(const scf_ctx *ctx);
void scf_exc_clear(scf_ctx *ctx);
void scf_raise(scf_ctx *ctx, const char *cls);
bool scf_exc_is(const scf_ctx *ctx, const char *cls);

/* Ported flows/leaves — PoP credits live in the .c file. */
json_t *scf_any_filter_args(scf_ctx *ctx, const json_t *args);
json_t *scf_prune_args(scf_ctx *ctx, const json_t *filters);
json_t *scf_confirm_or_exit_code(scf_ctx *ctx, const char *prompt,
                                 const char *declined, const char *unattended);
json_t *scf_not_found(scf_ctx *ctx, const json_t *session_id);
json_t *scf_print_empty_store(scf_ctx *ctx, const char *action, const json_t *args);
json_t *scf_delete(scf_ctx *ctx, const char *session_id, const json_t *fences);
json_t *scf_cmd_delete(scf_ctx *ctx, const json_t *args);
json_t *scf_cmd_rename(scf_ctx *ctx, const json_t *args);
json_t *scf_cmd_pin(scf_ctx *ctx, const json_t *args, bool pinning);
json_t *scf_print_dry_run_preview(scf_ctx *ctx, const json_t *candidates,
                                  const json_t *filters);
json_t *scf_note_pinned_skipped(scf_ctx *ctx, const json_t *filters, const char *action);
json_t *scf_output_file_in_dir(scf_ctx *ctx, const json_t *output, const char *default_name);
json_t *scf_write_output(scf_ctx *ctx, const json_t *output, const char *text,
                         const char *summary);
json_t *scf_render_only(scf_ctx *ctx, const json_t *args, const json_t *sessions);
json_t *scf_render_html(scf_ctx *ctx, const json_t *args, const json_t *sessions);
json_t *scf_export_flat(scf_ctx *ctx, const char *kind, json_t *args);
json_t *scf_print_recovery_verdict(scf_ctx *ctx, const json_t *report,
                                   const json_t *output, bool allow_partial);
json_t *scf_repair_prompts_pin_names(scf_ctx *ctx, const json_t *record);

#ifdef __cplusplus
}
#endif

#endif /* PORT_SESSIONS_CMD_FLOW_H */
