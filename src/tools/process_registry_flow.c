/*
 * process_registry_flow.c — background-process lifecycle FLOW cores of
 * tools/process_registry.py. See process_registry_flow.h for the state model,
 * the universal seam contract and the seam-name -> live-call table.
 *
 * Decision sequences only: every host probe, checkpoint write, scope stop,
 * signal, env exec, decoder flush, thread start, queue put, config/scope read,
 * redaction and log enters through prf_seams_t.call. Python truthiness,
 * or-chains, short-circuit order, dict insertion order and exception
 * suppress/propagate semantics are transcribed exactly.
 */

#include "process_registry_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *xstrdup_flow(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* ── trace recorder ── */
void prf_trace_add(prf_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
void prf_trace_free(prf_trace_t *t) {
    if (t && t->events) { json_free(t->events); t->events = NULL; }
}

/* ── raise spec ── */
const char *prf_raise(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}
const char *prf_raise_msg(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return "";
    return json_get_str(r, "msg", "");
}

/* ── Python truthiness / None ── */
static bool prf_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool prf_is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

/* ── state accessors ── */
static json_t *prf_sess(json_t *st, const char *sid) {
    if (!st || !sid) return NULL;
    json_t *sessions = json_obj_get(st, "sessions");
    return sessions ? json_obj_get(sessions, sid) : NULL;
}
static json_t *prf_list(json_t *st, const char *key) {
    json_t *a = json_obj_get(st, key);
    if (!a) { a = json_array(); json_set(st, key, a); }
    return a;
}
static bool prf_id_has(const json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *n = arr->c.items[i];
        if (n && n->type == JSON_STRING && strcmp(n->str_val, id) == 0) return true;
    }
    return false;
}
static void prf_id_remove(json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *n = arr->c.items[i];
        if (n && n->type == JSON_STRING && strcmp(n->str_val, id) == 0) {
            json_free(arr->c.items[i]);
            for (size_t j = i; j + 1 < arr->c.count; j++) arr->c.items[j] = arr->c.items[j + 1];
            arr->c.count--;
            return;
        }
    }
}
/* dict assignment semantics: an existing id keeps its position, a new id appends. */
static void prf_id_set(json_t *arr, const char *id) {
    if (!arr || !id) return;
    if (prf_id_has(arr, id)) return;
    json_append(arr, json_string(id));
}

/* dict.update / ** unpack: later keys replace values in place (position kept). */
static void prf_dict_merge(json_t *dst, const json_t *src) {
    if (!dst || !src || src->type != JSON_OBJECT) return;
    for (size_t i = 0; i < src->c.count; i++)
        json_set(dst, src->c.keys[i], json_copy(src->c.items[i]));
}

/* ── UTF-8 codepoint len / tail (Python len / [-n:] on str) ── */
static size_t prf_utf8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;   /* count lead bytes only */
    return n;
}
/* malloc'd last n codepoints of s (n may exceed len -> whole string). */
static char *prf_utf8_tail(const char *s, size_t n) {
    if (!s) return NULL;
    size_t len = strlen(s);
    size_t cps = prf_utf8_len(s);
    size_t start = 0;
    if (n < cps) {
        size_t skip = cps - n;
        size_t seen = 0;
        for (size_t i = 0; i < len; i++) {
            if (((unsigned char)s[i] & 0xC0) != 0x80) {
                if (seen == skip) { start = i; break; }
                seen++;
            }
        }
    }
    size_t out_len = len - start;
    char *d = malloc(out_len + 1);
    if (d) { memcpy(d, s + start, out_len); d[out_len] = '\0'; }
    return d;
}

/* ── seam call helpers ── */
/* Takes ownership of args (frees it); returns the owned seam result. */
static json_t *prf_call(const prf_seams_t *s, const char *name, json_t *args) {
    json_t *a = args ? args : json_object();
    json_t *r = s->call(s->ctx, name, a);
    json_free(a);
    return r;
}
static bool prf_call_bool(const prf_seams_t *s, const char *name, json_t *args) {
    json_t *r = prf_call(s, name, args);
    bool v = r && r->type == JSON_BOOL && r->bool_val;
    json_free(r);
    return v;
}
/* args builders: None-valued keys are OMITTED (mirrored by the oracle). */
static void prf_arg(json_t *a, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void prf_arg_str(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void prf_arg_num(json_t *a, const char *k, double v) {
    json_set(a, k, json_number(v));
}
static void prf_arg_bool(json_t *a, const char *k, bool v) {
    json_set(a, k, json_bool(v));
}
static void prf_log(const prf_seams_t *s, const char *level, const char *msg) {
    json_t *a = json_object();
    prf_arg_str(a, "level", level);
    prf_arg_str(a, "msg", msg);
    json_free(prf_call(s, "log", a));
}
/* session._completion_event.set(): trace event + modeled state flag. */
static void prf_event_set(json_t *st, const char *sid, const prf_seams_t *s) {
    json_t *a = json_object();
    prf_arg_str(a, "session_id", sid);
    json_free(prf_call(s, "completion_event_set", a));
    json_t *sess = prf_sess(st, sid);
    if (sess) json_set(sess, "_completion_event_set", json_bool(true));
}
static void prf_write_checkpoint(const prf_seams_t *s) {
    json_free(prf_call(s, "write_checkpoint", NULL));
}

/* ── internal composition of the CLAIMED ProcessSession.append_output ──
 * (with self._lock: self._append_locked(text)). No PoP claim: append_output is
 * already ported; this is the lock+append composition the flows below reuse. */
static void prf_session_append(json_t *session, const char *text) {
    prf_append_locked(session, text);
}

/* ── internal composition of the unclaimed _stop_scope helper ──
 * (if not session.systemd_unit: return True; stopped = _stop_systemd_unit(unit);
 *  session._scope_stop_pending = not stopped; return stopped). Composed under
 * _signal_kill's flow claim; no separate PoP. */
static bool prf_stop_scope(json_t *session, const prf_seams_t *s) {
    const char *unit = json_get_str(session, "systemd_unit", "");
    if (!prf_truthy(json_obj_get(session, "systemd_unit"))) return true;
    json_t *a = json_object();
    prf_arg_str(a, "unit", unit);
    bool stopped = prf_call_bool(s, "stop_systemd_unit", a);
    json_set(session, "_scope_stop_pending", json_bool(!stopped));
    return stopped;
}

/* ═══ Reaping: detached host-PID fate ═══════════════════════════════════════ */

/* PoP: prf_detached_host_fate_flow @ tools/process_registry.py:ProcessRegistry._detached_host_fate */
char *prf_detached_host_fate_flow(json_t *st, const json_t *pid,
                                  const json_t *expected_start,
                                  const prf_seams_t *s, json_t **exc) {
    (void)st; (void)exc;
    /* if self._host_pid_is_ours(pid, expected_start): return "running" */
    json_t *a = json_object();
    prf_arg(a, "pid", pid);
    prf_arg(a, "expected_start", expected_start);
    if (prf_call_bool(s, "host_pid_is_ours", a)) return xstrdup_flow("running");
    /* if not pid or not self._is_host_pid_alive(pid): return "gone" */
    if (!prf_truthy(pid)) return xstrdup_flow("gone");
    a = json_object();
    prf_arg(a, "pid", pid);
    if (!prf_call_bool(s, "is_host_pid_alive", a)) return xstrdup_flow("gone");
    /* if expected_start is None: return "running" */
    if (prf_is_none(expected_start)) return xstrdup_flow("running");
    /* current = self._safe_host_start_time(pid) */
    a = json_object();
    prf_arg(a, "pid", pid);
    json_t *current = prf_call(s, "safe_host_start_time", a);
    /* if current is None or current == expected_start: return "running" */
    bool running = prf_is_none(current);
    if (!running && current->type == JSON_NUMBER && expected_start->type == JSON_NUMBER)
        running = current->num_val == expected_start->num_val;
    json_free(current);
    return xstrdup_flow(running ? "running" : "reused");
}

/* PoP: prf_close_reused_detached_flow @ tools/process_registry.py:ProcessRegistry._close_reused_detached */
void prf_close_reused_detached_flow(json_t *st, const char *sid, double now,
                                    const prf_seams_t *s, json_t **exc) {
    (void)exc;
    json_t *session = prf_sess(st, sid);
    if (!session) return;
    /* with session._lock: if session.exited: return; session.exited = True */
    if (prf_truthy(json_obj_get(session, "exited"))) return;
    json_set(session, "exited", json_bool(true));
    /* with self._lock: */
    json_t *running = prf_list(st, "running");
    if (prf_id_has(running, sid)) {
        json_set(session, "exited_at", json_number(now));   /* time.time() */
        prf_id_remove(running, sid);                        /* _running.pop(id, None) */
    }
    prf_id_set(prf_list(st, "finished"), sid);              /* _finished[id] = session */
    prf_write_checkpoint(s);
    prf_event_set(st, sid, s);                              /* _completion_event.set() */
}

/* PoP: prf_prune_uncollected_detached_flow @ tools/process_registry.py:ProcessRegistry._prune_uncollected_detached */
json_t *prf_prune_uncollected_detached_flow(json_t *st, const char *sid,
                                            const prf_seams_t *s, json_t **exc) {
    (void)exc;
    json_t *session = prf_sess(st, sid);
    if (!session) return NULL;
    /* with session._lock: session.exited = True */
    json_set(session, "exited", json_bool(true));
    /* with self._lock: */
    prf_id_remove(prf_list(st, "running"), sid);            /* _running.pop(id, None) */
    bool has_unit = prf_truthy(json_obj_get(session, "systemd_unit"));
    if (has_unit)
        prf_id_set(prf_list(st, "finished"), sid);          /* _finished[id] = session */
    else
        prf_id_remove(prf_list(st, "finished"), sid);       /* _finished.pop(id, None) */
    prf_write_checkpoint(s);
    prf_event_set(st, sid, s);
    /* return session if session.systemd_unit else None */
    return has_unit ? json_string(sid) : NULL;
}

/* PoP: prf_uncollected_gone @ tools/process_registry.py:ProcessRegistry._uncollected_gone */
bool prf_uncollected_gone(json_t *st, const char *sid,
                          const prf_seams_t *s, json_t **exc) {
    (void)exc;
    /* bool(session is not None and session.exited and session.exit_code is None
     *      and session.detached and session.pid_scope == "host"
     *      and not self._is_host_pid_alive(session.pid)) */
    json_t *session = prf_sess(st, sid);
    if (!session) return false;
    if (!prf_truthy(json_obj_get(session, "exited"))) return false;
    if (!prf_is_none(json_obj_get(session, "exit_code"))) return false;
    if (!prf_truthy(json_obj_get(session, "detached"))) return false;
    if (strcmp(json_get_str(session, "pid_scope", ""), "host") != 0) return false;
    json_t *a = json_object();
    prf_arg(a, "pid", json_obj_get(session, "pid"));
    bool alive = prf_call_bool(s, "is_host_pid_alive", a);
    return !alive;
}

/* ═══ Output-drain ordering ═════════════════════════════════════════════════ */

/* PoP: prf_append_locked @ tools/process_registry.py:ProcessSession._append_locked */
void prf_append_locked(json_t *session, const char *text) {
    if (!session || !text) return;
    const char *buf = json_get_str(session, "output_buffer", "");
    size_t blen = strlen(buf), tlen = strlen(text);
    char *joined = malloc(blen + tlen + 1);
    if (!joined) return;
    memcpy(joined, buf, blen);
    memcpy(joined + blen, text, tlen);
    joined[blen + tlen] = '\0';
    /* self.output_buffer += text; self.total_output_chars += len(text) */
    double total = json_get_num(session, "total_output_chars", 0) + (double)prf_utf8_len(text);
    json_set(session, "total_output_chars", json_number(total));
    /* if len(self.output_buffer) > self.max_output_chars:
     *     self.output_buffer = self.output_buffer[-self.max_output_chars:] */
    double maxd = json_get_num(session, "max_output_chars", 200000);
    size_t maxc = (maxd < 0) ? 0 : (size_t)maxd;
    if (prf_utf8_len(joined) > maxc) {
        char *tail = prf_utf8_tail(joined, maxc);
        json_set(session, "output_buffer", json_string(tail ? tail : ""));
        free(tail);
    } else {
        json_set(session, "output_buffer", json_string(joined));
    }
    free(joined);
}

/* PoP: prf_append_output_if_running_flow @ tools/process_registry.py:ProcessSession.append_output_if_running */
bool prf_append_output_if_running_flow(json_t *session, const char *text) {
    /* with self._lock: if self.exited: return False; self._append_locked(text) */
    if (!session) return false;
    if (prf_truthy(json_obj_get(session, "exited"))) return false;
    prf_append_locked(session, text);
    return true;   /* return True (outside the lock) */
}

/* PoP: prf_mark_exited @ tools/process_registry.py:ProcessSession.mark_exited */
bool prf_mark_exited(json_t *session, const json_t *exit_code,
                     const char *reason, const char *source) {
    if (!session) return false;
    /* if self.exited: return False */
    if (prf_truthy(json_obj_get(session, "exited"))) return false;
    /* self.exited, self.exit_code, self.completion_reason = True, exit_code, reason */
    json_set(session, "exited", json_bool(true));
    json_set(session, "exit_code", exit_code ? json_copy(exit_code) : json_null());
    json_set(session, "completion_reason", json_string(reason ? reason : "exited"));
    /* self.termination_source = source or self.termination_source */
    const char *keep = json_get_str(session, "termination_source", "");
    const char *src = (source && source[0]) ? source : keep;
    json_set(session, "termination_source", json_string(src));
    return true;
}
/* ═══ Output-drain ordering (continued) ═════════════════════════════════════ */

/* PoP: prf_ingest_output_flow @ tools/process_registry.py:ProcessRegistry._ingest_output */
void prf_ingest_output_flow(json_t *st, const char *sid, const char *text,
                            bool unless_exited, const prf_seams_t *s, json_t **exc) {
    (void)exc;
    json_t *session = prf_sess(st, sid);
    if (!session) return;
    /* if not unless_exited: session.append_output(text)
     * elif not session.append_output_if_running(text): return */
    if (!unless_exited) {
        prf_session_append(session, text);
    } else if (!prf_append_output_if_running_flow(session, text)) {
        return;
    }
    /* self._check_watch_patterns(session, text); self._emit_output(session, text) */
    json_t *a = json_object();
    prf_arg_str(a, "session_id", sid); prf_arg_str(a, "text", text);
    json_free(prf_call(s, "check_watch_patterns", a));
    a = json_object();
    prf_arg_str(a, "session_id", sid); prf_arg_str(a, "text", text);
    json_free(prf_call(s, "emit_output", a));
}

/* PoP: prf_finish_exited_flow @ tools/process_registry.py:ProcessRegistry._finish_exited */
bool prf_finish_exited_flow(json_t *st, const char *sid, const json_t *exit_code,
                            const char *reason, const char *source,
                            const prf_seams_t *s, json_t **exc) {
    (void)exc;
    json_t *session = prf_sess(st, sid);
    /* with session._lock: recorded = session.mark_exited(exit_code, reason, source) */
    bool recorded = prf_mark_exited(session, exit_code, reason, source);
    /* self._move_to_finished(session) */
    json_t *a = json_object();
    prf_arg_str(a, "session_id", sid);
    json_free(prf_call(s, "move_to_finished", a));
    return recorded;
}

/* PoP: prf_finish_reader_flow @ tools/process_registry.py:ProcessRegistry._finish_reader */
void prf_finish_reader_flow(json_t *st, const char *sid, const char *label,
                            const prf_seams_t *s, json_t **exc) {
    /* with suppress(Exception): tail = decoder.decode(b"", final=True); if tail: append(tail) */
    json_t *tail = prf_call(s, "decoder_flush", NULL);
    if (prf_raise(tail)) { json_free(tail); tail = NULL; }   /* suppressed */
    if (prf_truthy(tail)) {
        json_t *a = json_object();
        prf_arg_str(a, "session_id", sid);
        prf_arg_str(a, "text", tail->str_val);
        json_t *ar = prf_call(s, "append", a);
        json_free(ar);   /* append runs inside the suppress block: any raise suppressed */
    }
    json_free(tail);
    /* try: wait() */
    json_t *a = json_object();
    prf_arg_str(a, "session_id", sid);
    json_t *wr = prf_call(s, "wait", a);
    if (prf_raise(wr)) {
        const char *msg = prf_raise_msg(wr);
        char buf[8192];
        /* if exit_code() is None: warn "leaving tracked"; return */
        a = json_object(); prf_arg_str(a, "session_id", sid);
        json_t *ec = prf_call(s, "exit_code", a);
        if (prf_is_none(ec)) {
            json_free(ec);
            snprintf(buf, sizeof buf, "%s wait failed; leaving process tracked: %s", label, msg);
            prf_log(s, "warning", buf);
            json_free(wr);
            return;
        }
        json_free(ec);
        snprintf(buf, sizeof buf, "%s wait failed; recording known exit status: %s", label, msg);
        prf_log(s, "warning", buf);
        json_free(wr);
    } else {
        json_free(wr);
    }
    /* self._finish_exited(session, exit_code()) */
    a = json_object(); prf_arg_str(a, "session_id", sid);
    json_t *ec2 = prf_call(s, "exit_code", a);
    prf_finish_exited_flow(st, sid, ec2, "exited", "", s, exc);
    json_free(ec2);
}

/* ═══ Release / publish ═════════════════════════════════════════════════════ */

/* PoP: prf_release_finished_handles_flow @ tools/process_registry.py:ProcessRegistry._release_finished_handles */
void prf_release_finished_handles_flow(json_t *st, const char *sid, bool is_windows,
                                       const char *current_thread_id,
                                       const prf_seams_t *s, json_t **exc) {
    json_t *session = prf_sess(st, sid);
    if (!session) return;
    /* proc = session.process; if proc is not None: close stdout/stderr/stdin */
    json_t *proc = json_obj_get(session, "process");
    if (!prf_is_none(proc)) {
        const char *streams[3] = { "stdout", "stderr", "stdin" };
        for (int i = 0; i < 3; i++) {
            json_t *stream = json_obj_get(proc, streams[i]);
            if (!prf_is_none(stream)) {
                /* with suppress(OSError, ValueError): stream.close() */
                json_t *a = json_object();
                prf_arg_str(a, "session_id", sid); prf_arg_str(a, "stream", streams[i]);
                json_t *r = prf_call(s, "stream_close", a);
                const char *kind = prf_raise(r);
                if (kind && strcmp(kind, "OSError") != 0 && strcmp(kind, "ValueError") != 0) {
                    *exc = r;   /* any other exception propagates */
                    return;
                }
                json_free(r);   /* OSError / ValueError suppressed (or no raise) */
            }
        }
    }
    /* if session._pty is not None: */
    json_t *pty = json_obj_get(session, "_pty");
    if (!prf_is_none(pty)) {
        /* reader = session._reader_thread;
         * if (not _IS_WINDOWS and reader is not None and reader.is_alive()
         *         and reader is not threading.current_thread()): return */
        json_t *reader = json_obj_get(session, "_reader_thread");
        bool defer = false;
        if (!is_windows && !prf_is_none(reader)) {
            bool alive = prf_truthy(json_obj_get(reader, "alive"));
            const char *rid = json_get_str(reader, "id", "");
            bool same = current_thread_id && strcmp(rid, current_thread_id) == 0;
            if (alive && !same) defer = true;
        }
        if (defer) return;
        /* with suppress(Exception): session._pty.close() */
        json_t *a = json_object(); prf_arg_str(a, "session_id", sid);
        json_free(prf_call(s, "pty_close", a));
    }
}

/* key_order: the notification's insertion order as an ordered list (the dict
 * itself compares unordered; this list pins the live ** unpack order). */
static json_t *prf_key_order(const json_t *obj) {
    json_t *a = json_array();
    if (obj && obj->type == JSON_OBJECT)
        for (size_t i = 0; i < obj->c.count; i++) json_append(a, json_string(obj->c.keys[i]));
    return a;
}

/* PoP: prf_publish_finished_flow @ tools/process_registry.py:ProcessRegistry._publish_finished */
void prf_publish_finished_flow(json_t *st, const char *sid, bool was_running,
                               bool is_windows, const char *current_thread_id,
                               const prf_seams_t *s, json_t **exc) {
    json_t *session = prf_sess(st, sid);
    /* self._release_finished_handles(session) (real; may propagate) */
    prf_release_finished_handles_flow(st, sid, is_windows, current_thread_id, s, exc);
    if (*exc) return;
    /* self._write_checkpoint() */
    prf_write_checkpoint(s);
    /* if was_running and session.notify_on_complete: */
    if (was_running && prf_truthy(json_obj_get(session, "notify_on_complete"))) {
        json_t *n = json_object();
        json_set(n, "type", json_string("completion"));
        json_set(n, "session_id", json_string(sid));
        json_set(n, "session_key", json_string(json_get_str(session, "session_key", "")));
        json_set(n, "task_id", json_string(json_get_str(session, "task_id", "")));
        json_set(n, "owner_task_id", json_string(json_get_str(session, "owner_task_id", "")));
        /* **producer_fields(session.sandbox_key) */
        json_t *a = json_object();
        prf_arg_str(a, "sandbox_key", json_get_str(session, "sandbox_key", ""));
        json_t *pf = prf_call(s, "producer_fields", a);
        if (pf) { prf_dict_merge(n, pf); json_free(pf); }
        json_set(n, "command", json_string(json_get_str(session, "command", "")));
        /* **({"handoff_note": session.handoff_note} if session.handoff_note else {}) */
        if (prf_truthy(json_obj_get(session, "handoff_note")))
            json_set(n, "handoff_note", json_string(json_get_str(session, "handoff_note", "")));
        /* **self._exit_fields(session) */
        a = json_object(); prf_arg_str(a, "session_id", sid);
        json_t *ef = prf_call(s, "exit_fields", a);
        if (ef) { prf_dict_merge(n, ef); json_free(ef); }
        /* **_completion_output(session) */
        a = json_object(); prf_arg_str(a, "session_id", sid);
        json_t *co = prf_call(s, "completion_output", a);
        if (co) { prf_dict_merge(n, co); json_free(co); }
        json_set(n, "started_at", json_number(json_get_num(session, "started_at", 0)));
        /* _redact_process_result(notification) (record only, no mutation) */
        a = json_object(); prf_arg(a, "notification", n);
        json_set(a, "key_order", prf_key_order(n));
        json_free(prf_call(s, "redact_result", a));
        /* self.completion_queue.put(notification) (record + modeled state append) */
        a = json_object(); prf_arg(a, "notification", n);
        json_set(a, "key_order", prf_key_order(n));
        json_free(prf_call(s, "queue_put", a));
        json_append(prf_list(st, "completion_queue"), json_copy(n));
        json_free(n);
    }
}

/* ═══ Ownership fences ══════════════════════════════════════════════════════ */

/* PoP: prf_transfer_ownership_flow @ tools/process_registry.py:ProcessRegistry.transfer_ownership */
json_t *prf_transfer_ownership_flow(json_t *st, const char *session_id,
                                    const char *from_owner, const char *to_owner,
                                    const char *to_task_id, const char *to_session_key,
                                    const char *note, const prf_seams_t *s, json_t **exc) {
    (void)exc;
    /* session = self.get(session_id) */
    json_t *a = json_object(); prf_arg_str(a, "session_id", session_id);
    json_t *gr = prf_call(s, "get", a);
    const char *rid = (gr && gr->type == JSON_STRING) ? gr->str_val : NULL;
    json_t *session = rid ? prf_sess(st, rid) : NULL;
    json_free(gr);
    /* with self._lock: if session is None or session.exited
     *     or session.owner_task_id != from_owner: return None */
    if (!session) return NULL;
    if (prf_truthy(json_obj_get(session, "exited"))) return NULL;
    if (strcmp(json_get_str(session, "owner_task_id", ""), from_owner ? from_owner : "") != 0) return NULL;
    /* session.owner_task_id = to_owner; task_id; session_key; handoff_note = note */
    json_set(session, "owner_task_id", json_string(to_owner ? to_owner : ""));
    json_set(session, "task_id", json_string(to_task_id ? to_task_id : ""));
    json_set(session, "session_key", json_string(to_session_key ? to_session_key : ""));
    json_set(session, "handoff_note", json_string(note ? note : ""));
    return json_string(rid);
}

/* PoP: prf_unread_completions_owned_by @ tools/process_registry.py:ProcessRegistry.unread_completions_owned_by */
json_t *prf_unread_completions_owned_by(json_t *st, const char *owner_task_id) {
    json_t *out = json_array();
    json_t *fin = prf_list(st, "finished");
    json_t *consumed = prf_list(st, "completion_consumed");
    json_t *polled = prf_list(st, "poll_observed");
    const char *owner = owner_task_id ? owner_task_id : "";
    /* [s for s in self._finished.values() if owner == ... and notify
     *     and exit_code is not None and id not in consumed and id not in polled] */
    for (size_t i = 0; i < fin->c.count; i++) {
        const json_t *idn = fin->c.items[i];
        if (!idn || idn->type != JSON_STRING) continue;
        const char *id = idn->str_val;
        json_t *sess = prf_sess(st, id);
        if (!sess) continue;
        if (strcmp(json_get_str(sess, "owner_task_id", ""), owner) != 0) continue;
        if (!prf_truthy(json_obj_get(sess, "notify_on_complete"))) continue;
        if (prf_is_none(json_obj_get(sess, "exit_code"))) continue;
        if (prf_id_has(consumed, id)) continue;
        if (prf_id_has(polled, id)) continue;
        json_append(out, json_string(id));
    }
    return out;
}

/* ═══ Register / adopt ══════════════════════════════════════════════════════ */

/* PoP: prf_track_started_flow @ tools/process_registry.py:ProcessRegistry._track_started */
void prf_track_started_flow(json_t *st, const char *sid, const char *reader_name,
                            const prf_seams_t *s, json_t **exc) {
    json_t *session = prf_sess(st, sid);
    /* reader = threading.Thread(target=copy_context().run, ..., name=reader_name) */
    json_free(prf_call(s, "copy_context", NULL));
    json_t *a = json_object();
    prf_arg_str(a, "name", reader_name); prf_arg_bool(a, "daemon", true);
    json_free(prf_call(s, "thread_new", a));
    /* session._reader_thread = reader (alive until start) */
    json_t *rt = json_object();
    json_set(rt, "id", json_string(reader_name));
    json_set(rt, "name", json_string(reader_name));
    json_set(rt, "alive", json_bool(false));
    json_set(session, "_reader_thread", rt);
    /* with self._lock: self._prune_if_needed(); reader.start(); _running[id] = session */
    json_free(prf_call(s, "prune_if_needed", NULL));
    a = json_object(); prf_arg_str(a, "name", reader_name);
    json_t *sr = prf_call(s, "reader_start", a);
    if (prf_raise(sr)) { *exc = sr; return; }   /* no ghost entry, no checkpoint */
    json_free(sr);
    json_set(json_obj_get(session, "_reader_thread"), "alive", json_bool(true));
    prf_id_set(prf_list(st, "running"), sid);
    /* self._write_checkpoint() */
    prf_write_checkpoint(s);
}

/* PoP: prf_adopt_local_flow @ tools/process_registry.py:ProcessRegistry.adopt_local */
json_t *prf_adopt_local_flow(json_t *st, const json_t *proc, const char *command,
                             const json_t *cwd, const char *task_id,
                             const char *session_key, const char *owner_task_id,
                             const char *output_so_far, bool notify_on_complete,
                             const prf_seams_t *s, json_t **exc) {
    /* session = self._new_session(command, task_id, owner_task_id, session_key, cwd) */
    json_t *a = json_object();
    prf_arg_str(a, "command", command); prf_arg_str(a, "task_id", task_id);
    prf_arg_str(a, "owner_task_id", owner_task_id); prf_arg_str(a, "session_key", session_key);
    prf_arg(a, "cwd", cwd);
    json_t *session = prf_call(s, "new_session", a);   /* NEW owned session object */
    if (!session) return NULL;
    const char *sid = json_get_str(session, "id", "");
    json_t *sessions = json_obj_get(st, "sessions");
    if (!sessions) { sessions = json_object(); json_set(st, "sessions", sessions); }
    json_set(sessions, sid, session);   /* store takes ownership */
    /* session.process = proc; session.pid = proc.pid */
    json_set(session, "process", proc ? json_copy(proc) : json_null());
    json_t *pid = proc ? json_obj_get(proc, "pid") : NULL;
    json_set(session, "pid", pid ? json_copy(pid) : json_null());
    /* session.host_start_time = self._safe_host_start_time(session.pid) */
    a = json_object(); prf_arg(a, "pid", pid);
    json_t *hst = prf_call(s, "safe_host_start_time", a);
    json_set(session, "host_start_time", hst ? hst : json_null());
    /* session.notify_on_complete = notify_on_complete */
    json_set(session, "notify_on_complete", json_bool(notify_on_complete));
    /* handed_pgid = getattr(proc, "_hermes_owned_pgid", None) */
    json_t *hpg = proc ? json_obj_get(proc, "_hermes_owned_pgid") : NULL;
    /* self._take_handoff_group(session, proc) */
    a = json_object(); prf_arg_str(a, "session_id", sid); prf_arg(a, "proc", proc);
    json_free(prf_call(s, "take_handoff_group", a));
    /* if output_so_far: session.append_output(output_so_far) */
    if (output_so_far && output_so_far[0]) prf_session_append(session, output_so_far);
    /* try: self._track_started(session, self._reader_loop, f"proc-reader-{session.id}") */
    char rname[512];
    snprintf(rname, sizeof rname, "proc-reader-%s", sid);
    json_t *sub_exc = NULL;
    prf_track_started_flow(st, sid, rname, s, &sub_exc);
    if (sub_exc) {
        /* except Exception: self._return_handoff_group(session, proc, handed_pgid); raise */
        a = json_object();
        prf_arg_str(a, "session_id", sid); prf_arg(a, "proc", proc); prf_arg(a, "handed_pgid", hpg);
        json_free(prf_call(s, "return_handoff_group", a));
        *exc = sub_exc;
        return NULL;
    }
    return json_string(sid);
}

/* ═══ Persistence branch ════════════════════════════════════════════════════ */

/* PoP: prf_restore_completions_flow @ tools/process_registry.py:ProcessRegistry.restore_completions */
int prf_restore_completions_flow(json_t *st, const prf_seams_t *s, json_t **exc) {
    (void)exc;
    /* if self._completions_restored or hosted_scope() is not None: return 0 */
    if (prf_truthy(json_obj_get(st, "completions_restored"))) return 0;
    json_t *hs = prf_call(s, "hosted_scope", NULL);
    bool hosted = !prf_is_none(hs);   /* is not None (identity, not truthiness) */
    json_free(hs);
    if (hosted) return 0;
    /* self._completions_restored = True */
    json_set(st, "completions_restored", json_bool(true));
    /* token = set_hermes_home_override(None) */
    json_t *a = json_object();   /* value None omitted */
    json_t *token = prf_call(s, "set_home_override", a);
    int ret = 0;
    /* try: return restore_undelivered_completions(self.completion_queue) */
    json_t *rr = prf_call(s, "restore_undelivered", NULL);
    if (prf_raise(rr)) {
        /* except Exception as exc: logger.warning(...); return 0 */
        char buf[8192];
        snprintf(buf, sizeof buf, "Could not restore async delegation completions: %s", prf_raise_msg(rr));
        prf_log(s, "warning", buf);
        json_free(rr);
        ret = 0;
    } else {
        ret = (rr && rr->type == JSON_NUMBER) ? (int)rr->num_val : 0;
        json_free(rr);
    }
    /* finally: reset_hermes_home_override(token) */
    a = json_object(); prf_arg(a, "token", token);
    json_free(prf_call(s, "reset_home_override", a));
    json_free(token);
    return ret;
}

/* ═══ Kill decision chain / error paths ═════════════════════════════════════ */

/* PoP: prf_signal_kill_flow @ tools/process_registry.py:ProcessRegistry._signal_kill */
json_t *prf_signal_kill_flow(json_t *st, const char *sid, bool consume_output,
                             double now, const prf_seams_t *s, json_t **exc) {
    json_t *session = prf_sess(st, sid);
    if (!session) return NULL;
    json_t *pty = json_obj_get(session, "_pty");
    json_t *proc = json_obj_get(session, "process");
    json_t *env = json_obj_get(session, "env_ref");
    json_t *pid = json_obj_get(session, "pid");
    /* if session._pty: try terminate(force=True) except: if pid: os.kill(pid, SIGTERM) */
    if (!prf_is_none(pty)) {
        json_t *a = json_object();
        prf_arg_str(a, "session_id", sid); prf_arg_bool(a, "force", true);
        json_t *tr = prf_call(s, "pty_terminate", a);
        if (prf_raise(tr)) {
            json_free(tr);
            if (prf_truthy(pid)) {   /* pid 0 is falsy */
                a = json_object(); prf_arg(a, "pid", pid); prf_arg_num(a, "sig", 15);
                json_t *kr = prf_call(s, "os_kill", a);
                if (prf_raise(kr)) { *exc = kr; return NULL; }
                json_free(kr);
            }
        } else {
            json_free(tr);
        }
    } else if (!prf_is_none(proc)) {
        /* elif session.process: self._terminate_host_pid(proc.pid, host_start_time) */
        json_t *a = json_object();
        prf_arg(a, "pid", json_obj_get(proc, "pid"));
        prf_arg(a, "expected_start", json_obj_get(session, "host_start_time"));
        json_free(prf_call(s, "terminate_host_pid", a));
    } else if (!prf_is_none(env) && prf_truthy(pid)) {
        /* elif session.env_ref and session.pid: env_ref.execute(f"kill {pid} 2>/dev/null", ...) */
        char cmd[128];
        snprintf(cmd, sizeof cmd, "kill %.0f 2>/dev/null", pid->num_val);
        json_t *a = json_object();
        prf_arg_str(a, "command", cmd); prf_arg_str(a, "cwd", "/"); prf_arg_num(a, "timeout", 5);
        json_t *er = prf_call(s, "env_execute", a);
        if (prf_raise(er)) { *exc = er; return NULL; }
        json_free(er);
    } else if (prf_truthy(json_obj_get(session, "detached"))
               && strcmp(json_get_str(session, "pid_scope", ""), "host") == 0
               && prf_truthy(pid)) {
        /* elif session.detached and pid_scope == "host" and pid: */
        json_t *hst = json_obj_get(session, "host_start_time");
        json_t *sub_exc = NULL;
        char *fate = prf_detached_host_fate_flow(st, pid, hst, s, &sub_exc);
        if (sub_exc) { *exc = sub_exc; free(fate); return NULL; }
        bool running = fate && strcmp(fate, "running") == 0;
        free(fate);
        if (!running) {
            /* scope_stopped = self._stop_scope(session) */
            bool scope_stopped = prf_stop_scope(session, s);
            /* with session._lock: output = _completion_output(session) */
            json_t *a = json_object(); prf_arg_str(a, "session_id", sid);
            json_t *output = prf_call(s, "completion_output", a);
            /* if consume_output: self._completion_consumed.add(session.id) */
            if (consume_output) prf_id_set(prf_list(st, "completion_consumed"), sid);
            /* self._close_reused_detached(session) */
            json_t *sub2 = NULL;
            prf_close_reused_detached_flow(st, sid, now, s, &sub2);
            if (sub2) { *exc = sub2; json_free(output); return NULL; }
            /* result = {"status": "already_exited", "exit_code": session.exit_code, **output} */
            json_t *result = json_object();
            json_set(result, "status", json_string("already_exited"));
            json_t *ec = json_obj_get(session, "exit_code");
            json_set(result, "exit_code", ec ? json_copy(ec) : json_null());
            if (output) { prf_dict_merge(result, output); json_free(output); }
            /* if not scope_stopped: result["scope_stop_failed"] = session.systemd_unit */
            if (!scope_stopped)
                json_set(result, "scope_stop_failed", json_string(json_get_str(session, "systemd_unit", "")));
            return result;
        }
        /* identity proven: self._terminate_host_pid(session.pid) (no expected_start) */
        json_t *a = json_object(); prf_arg(a, "pid", pid);
        json_free(prf_call(s, "terminate_host_pid", a));
    } else {
        /* else: return {"status": "error", "error": "Recovered process cannot be killed ..."} */
        json_t *r = json_object();
        json_set(r, "status", json_string("error"));
        json_set(r, "error", json_string(
            "Recovered process cannot be killed after restart because "
            "its original runtime handle is no longer available"));
        return r;
    }
    return NULL;
}
