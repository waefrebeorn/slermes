/*
 * process_registry_flow.h — background-process lifecycle FLOW cores of
 * tools/process_registry.py (register / adopt / release decision chains,
 * stale-process reaping, ownership fences, output-drain ordering, persistence
 * branches, error paths).
 *
 * Ports the DECISION sequences (call order, branch conditions, early returns,
 * exception -> fallback / suppress paths, mutation order, dict insertion order)
 * of the live module. All live I/O (host-PID probes, checkpoint writes, scope
 * stops, signal delivery, env exec, decoder flush, thread start, queue puts,
 * config / scope reads, redaction, logging) enters through the universal seam
 * below; the cores perform no I/O.
 *
 * Sequences are observable through a prf_trace_t: the harness wires the seam to
 * a recording stub that appends one JSON event per call (arguments + scripted
 * result) in call order; the oracle compares that ordered event list against
 * recording monkeypatch stubs driving the LIVE Python with the same scripts.
 *
 * ── Registry state model (json_t *st, mutated in place) ─────────────────────
 *   {
 *     "sessions": { "<id>": <session object> },   canonical store (identity)
 *     "running":  [ "<id>", ... ],                insertion-ordered id list
 *     "finished": [ "<id>", ... ],                insertion-ordered id list
 *     "completion_consumed": [ "<id>", ... ],     set (sorted on dump)
 *     "poll_observed":       [ "<id>", ... ],     set (sorted on dump)
 *     "completion_queue":    [ <event>, ... ],
 *     "completions_restored": <bool>
 *   }
 * Python dicts share session OBJECT identity between _running / _finished; the
 * id-list + canonical-store model reproduces that without pointer aliasing:
 * a move pop's the id from one list and sets it in the other (an existing
 * finished id keeps its position, mirroring dict assignment).
 *
 * ── Universal seam ──────────────────────────────────────────────────────────
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * name identifies the live call (table below); args is a BORROWED object of the
 * call's arguments; the result is a NEW json_t the caller frees. NULL == Python
 * None. A scripted exception is returned as {"__raise__": "<Kind>", "msg": ...};
 * prf_raise() detects it. Void seams return NULL. The harness records
 * {ev:name, args:<copy>, ret:<script>} per call; a scripted JSON null returns a
 * NULL pointer (NOT a json_null node) so truthiness tests read it as None.
 *
 * Seam name -> live call (args keys; ret):
 *   host_pid_is_ours      ProcessRegistry._host_pid_is_ours {pid,expected_start}; bool
 *   is_host_pid_alive     ProcessRegistry._is_host_pid_alive {pid}; bool
 *   safe_host_start_time  ProcessRegistry._safe_host_start_time {pid}; int|null
 *   write_checkpoint      ProcessRegistry._write_checkpoint {}; null
 *   move_to_finished      ProcessRegistry._move_to_finished {session_id}; bool
 *   check_watch_patterns  ProcessRegistry._check_watch_patterns {session_id,text}; null
 *   emit_output           ProcessRegistry._emit_output {session_id,text}; null
 *   prune_if_needed       ProcessRegistry._prune_if_needed {}; null
 *   get                   ProcessRegistry.get {session_id}; resolved id|null
 *   new_session           ProcessRegistry._new_session {command,task_id,owner_task_id,
 *                         session_key,cwd}; NEW session object (inserted into store)
 *   take_handoff_group    ProcessOwnershipMixin._take_handoff_group {session_id,proc}; null
 *   return_handoff_group  ProcessOwnershipMixin._return_handoff_group
 *                         {session_id,proc,handed_pgid}; null
 *   terminate_host_pid    ProcessTerminationMixin._terminate_host_pid {pid,expected_start}; null
 *   stop_systemd_unit     _stop_systemd_unit {unit}; bool
 *   completion_output     _completion_output {session_id}; dict
 *   exit_fields           ProcessRegistry._exit_fields {session_id}; dict
 *   producer_fields       sandbox_scope.producer_fields {sandbox_key}; dict
 *   redact_result         _redact_process_result {notification}; null (record only)
 *   queue_put             completion_queue.put {notification}; null (state append modeled)
 *   completion_event_set  session._completion_event.set {session_id}; null (flag modeled)
 *   hosted_scope          sandbox_scope.hosted_scope {}; string|null
 *   set_home_override     hermes_constants.set_hermes_home_override {value}; token
 *   reset_home_override   hermes_constants.reset_hermes_home_override {token}; null
 *   restore_undelivered   async_delegation.restore_undelivered_completions {}; int (may raise)
 *   env_execute           env_ref.execute {command,cwd,timeout}; null (may raise)
 *   os_kill               os.kill {pid,sig}; null (may raise)
 *   stream_close          proc.<stream>.close {session_id,stream}; null (may raise)
 *   pty_close             session._pty.close {session_id}; null (may raise)
 *   pty_terminate         session._pty.terminate {session_id,force}; null (may raise)
 *   decoder_flush         decoder.decode(b"",final=True) {}; string (may raise)
 *   append                reader append callback {session_id,text}; null (may raise)
 *   wait                  proc.wait / pty.wait {session_id}; null (may raise)
 *   exit_code             returncode / exitstatus probe {session_id}; int|null
 *   copy_context          contextvars.copy_context {}; null
 *   thread_new            threading.Thread(...) {name,daemon}; null
 *   reader_start          reader.start() {name}; null (may raise)
 *   log                   logger.<level> {level,msg}; null
 *
 * JSON values are libjson json_t. Ownership is documented per function.
 * Python truthiness / or-chain / short-circuit order is preserved exactly.
 */

#ifndef PROCESS_REGISTRY_FLOW_H
#define PROCESS_REGISTRY_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Trace recorder (harness infrastructure, no Python counterpart) ─────────
 * events is a JSON_ARRAY of event objects. prf_trace_add takes ownership of ev
 * (frees it when t is NULL). The trace never leaves the caller. */
typedef struct prf_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} prf_trace_t;

void prf_trace_add(prf_trace_t *t, json_t *ev);
void prf_trace_free(prf_trace_t *t);

/* ── Universal seam ── */
typedef struct prf_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} prf_seams_t;

/* Raise detection: returns the kind string when r is a {"__raise__":...} spec,
 * else NULL. prf_raise_msg returns the scripted message ("" when absent). */
const char *prf_raise(const json_t *r);
const char *prf_raise_msg(const json_t *r);

/* ── Reaping: detached host-PID fate ────────────────────────────────────────
 * tools/process_registry.py:ProcessRegistry._detached_host_fate.
 * pid / expected_start are borrowed JSON number-or-null nodes. Returns a
 * malloc'd "running" / "reused" / "gone" (caller frees). */
/* PoP: prf_detached_host_fate_flow @ tools/process_registry.py:ProcessRegistry._detached_host_fate */
char *prf_detached_host_fate_flow(json_t *st, const json_t *pid,
                                  const json_t *expected_start,
                                  const prf_seams_t *s, json_t **exc);

/* tools/process_registry.py:ProcessRegistry._close_reused_detached.
 * now is the scripted time.time() used for exited_at. Void. */
/* PoP: prf_close_reused_detached_flow @ tools/process_registry.py:ProcessRegistry._close_reused_detached */
void prf_close_reused_detached_flow(json_t *st, const char *sid, double now,
                                    const prf_seams_t *s, json_t **exc);

/* tools/process_registry.py:ProcessRegistry._prune_uncollected_detached.
 * Returns a NEW json_string(sid) when the session has a systemd scope (kept
 * reachable), else NULL (Python None). Caller frees. */
/* PoP: prf_prune_uncollected_detached_flow @ tools/process_registry.py:ProcessRegistry._prune_uncollected_detached */
json_t *prf_prune_uncollected_detached_flow(json_t *st, const char *sid,
                                            const prf_seams_t *s, json_t **exc);

/* tools/process_registry.py:ProcessRegistry._uncollected_gone.
 * sid NULL models session None. Returns the bool verdict. */
/* PoP: prf_uncollected_gone @ tools/process_registry.py:ProcessRegistry._uncollected_gone */
bool prf_uncollected_gone(json_t *st, const char *sid,
                          const prf_seams_t *s, json_t **exc);

/* ── Output-drain ordering ──────────────────────────────────────────────────
 * tools/process_registry.py:ProcessSession._append_locked. Pure rolling-tail
 * buffer append (UTF-8 codepoint len / slice). Mutates session in place. */
/* PoP: prf_append_locked @ tools/process_registry.py:ProcessSession._append_locked */
void prf_append_locked(json_t *session, const char *text);

/* tools/process_registry.py:ProcessSession.append_output_if_running.
 * Exited gate decided under the (modeled) session lock. Returns whether appended. */
/* PoP: prf_append_output_if_running_flow @ tools/process_registry.py:ProcessSession.append_output_if_running */
bool prf_append_output_if_running_flow(json_t *session, const char *text);

/* tools/process_registry.py:ProcessSession.mark_exited. First-observer-wins;
 * exit_code is a borrowed number-or-null node. source "" keeps the existing
 * termination_source (or-chain). Returns whether it recorded. */
/* PoP: prf_mark_exited @ tools/process_registry.py:ProcessSession.mark_exited */
bool prf_mark_exited(json_t *session, const json_t *exit_code,
                     const char *reason, const char *source);

/* tools/process_registry.py:ProcessRegistry._ingest_output. Buffer then watch
 * then live-emit; unless_exited drops the chunk atomically with a kill. Void. */
/* PoP: prf_ingest_output_flow @ tools/process_registry.py:ProcessRegistry._ingest_output */
void prf_ingest_output_flow(json_t *st, const char *sid, const char *text,
                            bool unless_exited, const prf_seams_t *s, json_t **exc);

/* tools/process_registry.py:ProcessRegistry._finish_exited. mark_exited under
 * the session lock, then move_to_finished (seamed). Returns whether recorded. */
/* PoP: prf_finish_exited_flow @ tools/process_registry.py:ProcessRegistry._finish_exited */
bool prf_finish_exited_flow(json_t *st, const char *sid, const json_t *exit_code,
                            const char *reason, const char *source,
                            const prf_seams_t *s, json_t **exc);

/* tools/process_registry.py:ProcessRegistry._finish_reader. Decoder flush
 * (suppressed), wait (exception -> known/unknown exit branch), finish. Void. */
/* PoP: prf_finish_reader_flow @ tools/process_registry.py:ProcessRegistry._finish_reader */
void prf_finish_reader_flow(json_t *st, const char *sid, const char *label,
                            const prf_seams_t *s, json_t **exc);

/* ── Release / publish ──────────────────────────────────────────────────────
 * tools/process_registry.py:ProcessRegistry._release_finished_handles.
 * Close Popen streams (stdout,stderr,stdin; OSError/ValueError suppressed),
 * then the PTY master unless a live foreign reader still holds it. is_windows
 * and current_thread_id model _IS_WINDOWS / threading.current_thread(). Void. */
/* PoP: prf_release_finished_handles_flow @ tools/process_registry.py:ProcessRegistry._release_finished_handles */
void prf_release_finished_handles_flow(json_t *st, const char *sid, bool is_windows,
                                       const char *current_thread_id,
                                       const prf_seams_t *s, json_t **exc);

/* tools/process_registry.py:ProcessRegistry._publish_finished. Release handles,
 * checkpoint, then (was_running and notify_on_complete) build the completion
 * notification in live insertion order, redact, queue. Void. */
/* PoP: prf_publish_finished_flow @ tools/process_registry.py:ProcessRegistry._publish_finished */
void prf_publish_finished_flow(json_t *st, const char *sid, bool was_running,
                               bool is_windows, const char *current_thread_id,
                               const prf_seams_t *s, json_t **exc);

/* ── Ownership fences ───────────────────────────────────────────────────────
 * tools/process_registry.py:ProcessRegistry.transfer_ownership. get (seamed)
 * then the running / exited / from_owner fence under the registry lock.
 * Returns a NEW json_string(sid) on transfer, else NULL. Caller frees. */
/* PoP: prf_transfer_ownership_flow @ tools/process_registry.py:ProcessRegistry.transfer_ownership */
json_t *prf_transfer_ownership_flow(json_t *st, const char *session_id,
                                    const char *from_owner, const char *to_owner,
                                    const char *to_task_id, const char *to_session_key,
                                    const char *note, const prf_seams_t *s, json_t **exc);

/* tools/process_registry.py:ProcessRegistry.unread_completions_owned_by.
 * Filter finished sessions by owner / notify / collected-exit / unread fences.
 * Returns a NEW JSON array of ids in finished insertion order. Caller frees. */
/* PoP: prf_unread_completions_owned_by @ tools/process_registry.py:ProcessRegistry.unread_completions_owned_by */
json_t *prf_unread_completions_owned_by(json_t *st, const char *owner_task_id);

/* ── Register / adopt ───────────────────────────────────────────────────────
 * tools/process_registry.py:ProcessRegistry._track_started. Register before the
 * reader can publish: copy_context, thread_new, prune, reader_start (may raise
 * -> no ghost entry), running insert, checkpoint. Void. */
/* PoP: prf_track_started_flow @ tools/process_registry.py:ProcessRegistry._track_started */
void prf_track_started_flow(json_t *st, const char *sid, const char *reader_name,
                            const prf_seams_t *s, json_t **exc);

/* tools/process_registry.py:ProcessRegistry.adopt_local. Take over a running
 * foreground Popen; on _track_started failure return the handoff group and
 * re-raise. proc is a borrowed Popen-double object. Returns NEW json_string(sid)
 * or NULL. Caller frees. */
/* PoP: prf_adopt_local_flow @ tools/process_registry.py:ProcessRegistry.adopt_local */
json_t *prf_adopt_local_flow(json_t *st, const json_t *proc, const char *command,
                             const json_t *cwd, const char *task_id,
                             const char *session_key, const char *owner_task_id,
                             const char *output_so_far, bool notify_on_complete,
                             const prf_seams_t *s, json_t **exc);

/* ── Persistence branch ─────────────────────────────────────────────────────
 * tools/process_registry.py:ProcessRegistry.restore_completions. Once-per-process
 * replay gated on _completions_restored / hosted_scope; home-override token set
 * and reset in finally; restore exception -> warn + 0. Returns the count. */
/* PoP: prf_restore_completions_flow @ tools/process_registry.py:ProcessRegistry.restore_completions */
int prf_restore_completions_flow(json_t *st, const prf_seams_t *s, json_t **exc);

/* ── Kill decision chain / error paths ──────────────────────────────────────
 * tools/process_registry.py:ProcessRegistry._signal_kill. PTY / local Popen tree
 * / sandbox exec / recovered host PID / no-handle error. Returns a NEW result
 * dict when the kill cannot proceed, else NULL (Python None). Caller frees. */
/* PoP: prf_signal_kill_flow @ tools/process_registry.py:ProcessRegistry._signal_kill */
json_t *prf_signal_kill_flow(json_t *st, const char *sid, bool consume_output,
                             double now, const prf_seams_t *s, json_t **exc);

#ifdef __cplusplus
}
#endif

#endif /* PROCESS_REGISTRY_FLOW_H */
