/*
 * t_port_process_registry_flow.c — oracle harness for the background-process
 * lifecycle FLOW cores ported in src/tools/process_registry_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires the universal seam to a SCRIPTED recording stub (in["io"][name]: a single
 * value reused, or an array consumed per call) that appends one {ev,args,ret}
 * trace event per call, in call order. Runs the flow on a COPY of in["state"]
 * (registry flows) or in["session"] (session flows) and emits
 * {"func","in","out":{"ret","exc","trace","state"|"session"}} per case. The
 * original "in" is echoed unmutated. tests/oracle_process_registry_flow.py drives
 * the LIVE Python with the same scripts and compares ordered trace + ret + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "libjson/json.h"
#include "process_registry_flow.h"

typedef struct {
    const json_t *in;
    prf_trace_t *trace;
    json_t *counts;   /* object: seam name -> call count (array scripts) */
} script_ctx_t;

/* ── generic scripted recording seam ── */
static json_t *sc_call(void *ctx, const char *name, const json_t *args) {
    script_ctx_t *c = ctx;
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *script = io ? json_obj_get(io, name) : NULL;
    const json_t *val = NULL;
    if (script && script->type == JSON_ARRAY) {
        double cnt = json_get_num(c->counts, name, 0);
        size_t idx = (size_t)cnt;
        if (script->c.count) {
            if (idx >= script->c.count) idx = script->c.count - 1;
            val = script->c.items[idx];
        }
        json_set(c->counts, name, json_number(cnt + 1));
    } else {
        val = script;   /* single value reused (NULL when absent) */
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    if (strcmp(name, "new_session") == 0) {
        /* new_session returns the session object; the trace ret is its id */
        json_set(e, "ret", json_string(val ? json_get_str(val, "id", "") : ""));
    } else {
        json_set(e, "ret", val ? json_copy(val) : json_null());
    }
    prf_trace_add(c->trace, e);
    /* a scripted JSON null returns a NULL pointer (None), not a json_null node */
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

/* ── session model defaults (mirrors ProcessSession; oracle seeds the same) ── */
static void set_def_str(json_t *s, const char *k, const char *v) {
    if (!json_has(s, k)) json_set(s, k, json_string(v));
}
static void set_def_num(json_t *s, const char *k, double v) {
    if (!json_has(s, k)) json_set(s, k, json_number(v));
}
static void set_def_bool(json_t *s, const char *k, bool v) {
    if (!json_has(s, k)) json_set(s, k, json_bool(v));
}
static void set_def_null(json_t *s, const char *k) {
    if (!json_has(s, k)) json_set(s, k, json_null());
}
static void normalize_session(json_t *s) {
    if (!s || s->type != JSON_OBJECT) return;
    set_def_str(s, "id", "");
    set_def_str(s, "command", "");
    set_def_str(s, "task_id", "");
    set_def_str(s, "owner_task_id", "");
    set_def_str(s, "session_key", "");
    set_def_str(s, "sandbox_key", "");
    set_def_null(s, "cwd");
    set_def_null(s, "pid");
    set_def_str(s, "pid_scope", "host");
    set_def_null(s, "host_start_time");
    set_def_num(s, "started_at", 0.0);
    set_def_bool(s, "exited", false);
    set_def_num(s, "exited_at", 0.0);
    set_def_null(s, "exit_code");
    set_def_str(s, "completion_reason", "exited");
    set_def_str(s, "termination_source", "");
    set_def_str(s, "output_buffer", "");
    set_def_num(s, "max_output_chars", 200000);
    set_def_num(s, "total_output_chars", 0);
    set_def_bool(s, "detached", false);
    set_def_str(s, "systemd_unit", "");
    set_def_str(s, "output_log", "");
    set_def_bool(s, "notify_on_complete", false);
    set_def_str(s, "handoff_note", "");
    set_def_bool(s, "persist_on_release", false);
    set_def_num(s, "completion_output_chars", 0);
    set_def_bool(s, "_finish_claimed", false);
    set_def_bool(s, "_scope_stop_pending", false);
    set_def_bool(s, "_completion_event_set", false);
    set_def_bool(s, "_reader_finish_requested", false);
    set_def_null(s, "_reader_thread");
    set_def_null(s, "process");
    set_def_null(s, "_pty");
    set_def_null(s, "env_ref");
}
static void normalize_state(json_t *st) {
    if (!st || st->type != JSON_OBJECT) return;
    json_t *sessions = json_obj_get(st, "sessions");
    if (sessions && sessions->type == JSON_OBJECT)
        for (size_t i = 0; i < sessions->c.count; i++) normalize_session(sessions->c.items[i]);
    const char *lists[] = { "running", "finished", "completion_consumed", "poll_observed", "completion_queue" };
    for (int i = 0; i < 5; i++) if (!json_has(st, lists[i])) json_set(st, lists[i], json_array());
    if (!json_has(st, "completions_restored")) json_set(st, "completions_restored", json_bool(false));
}
/* sort an id array in place (sets dump sorted on both sides) */
static void sort_id_array(json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 1; i < arr->c.count; i++) {
        json_t *key = arr->c.items[i];
        size_t j = i;
        while (j > 0 && arr->c.items[j - 1]->type == JSON_STRING && key->type == JSON_STRING
               && strcmp(arr->c.items[j - 1]->str_val, key->str_val) > 0) {
            arr->c.items[j] = arr->c.items[j - 1];
            j--;
        }
        arr->c.items[j] = key;
    }
}
static void dump_state(json_t *st) {
    sort_id_array(json_obj_get(st, "completion_consumed"));
    sort_id_array(json_obj_get(st, "poll_observed"));
}

/* ── input readers ── */
static const json_t *in_node(const json_t *in, const char *k) { return json_obj_get(in, k); }
static const char *in_str(const json_t *in, const char *k, const char *def) { return json_get_str(in, k, def); }
static bool in_bool(const json_t *in, const char *k) { return json_get_bool(in, k, false); }
static double in_num(const json_t *in, const char *k, double def) { return json_get_num(in, k, def); }

static bool is_session_func(const char *func) {
    return strcmp(func, "append_locked") == 0
        || strcmp(func, "append_output_if_running") == 0
        || strcmp(func, "mark_exited") == 0;
}

/* ── per-case dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, NULL, json_object() };
    prf_trace_t trace = { NULL };
    ctx.trace = &trace;
    prf_seams_t s = { &ctx, sc_call };
    json_t *exc = NULL;
    json_t *ret = NULL;
    bool ret_bool = false;
    int ret_int = 0;
    char *ret_str = NULL;
    json_t *state = NULL;
    json_t *session = NULL;

    if (is_session_func(func)) {
        const json_t *src = in_node(in, "session");
        session = src ? json_copy(src) : json_object();
        normalize_session(session);
        const char *text = in_str(in, "text", "");
        if (strcmp(func, "append_locked") == 0) {
            prf_append_locked(session, text);
        } else if (strcmp(func, "append_output_if_running") == 0) {
            ret_bool = prf_append_output_if_running_flow(session, text);
        } else { /* mark_exited */
            ret_bool = prf_mark_exited(session, in_node(in, "exit_code"),
                                       in_str(in, "reason", "exited"), in_str(in, "source", ""));
        }
        json_set(out, "session", session);
    } else {
        const json_t *src = in_node(in, "state");
        state = src ? json_copy(src) : json_object();
        normalize_state(state);
        const char *sid = in_str(in, "sid", NULL);
        double now = in_num(in, "now", 0.0);
        if (strcmp(func, "detached_host_fate") == 0) {
            ret_str = prf_detached_host_fate_flow(state, in_node(in, "pid"), in_node(in, "expected_start"), &s, &exc);
        } else if (strcmp(func, "close_reused_detached") == 0) {
            prf_close_reused_detached_flow(state, sid, now, &s, &exc);
        } else if (strcmp(func, "prune_uncollected_detached") == 0) {
            ret = prf_prune_uncollected_detached_flow(state, sid, &s, &exc);
        } else if (strcmp(func, "uncollected_gone") == 0) {
            ret_bool = prf_uncollected_gone(state, sid, &s, &exc);
        } else if (strcmp(func, "ingest_output") == 0) {
            prf_ingest_output_flow(state, sid, in_str(in, "text", ""), in_bool(in, "unless_exited"), &s, &exc);
        } else if (strcmp(func, "finish_exited") == 0) {
            ret_bool = prf_finish_exited_flow(state, sid, in_node(in, "exit_code"),
                                              in_str(in, "reason", "exited"), in_str(in, "source", ""), &s, &exc);
        } else if (strcmp(func, "finish_reader") == 0) {
            prf_finish_reader_flow(state, sid, in_str(in, "label", "Process"), &s, &exc);
        } else if (strcmp(func, "release_finished_handles") == 0) {
            prf_release_finished_handles_flow(state, sid, in_bool(in, "is_windows"),
                                              in_str(in, "current_thread_id", "main"), &s, &exc);
        } else if (strcmp(func, "publish_finished") == 0) {
            prf_publish_finished_flow(state, sid, in_bool(in, "was_running"), in_bool(in, "is_windows"),
                                      in_str(in, "current_thread_id", "main"), &s, &exc);
        } else if (strcmp(func, "transfer_ownership") == 0) {
            ret = prf_transfer_ownership_flow(state, in_str(in, "session_id", ""), in_str(in, "from_owner", ""),
                                              in_str(in, "to_owner", ""), in_str(in, "to_task_id", ""),
                                              in_str(in, "to_session_key", ""), in_str(in, "note", ""), &s, &exc);
        } else if (strcmp(func, "unread_completions_owned_by") == 0) {
            ret = prf_unread_completions_owned_by(state, in_str(in, "owner_task_id", ""));
        } else if (strcmp(func, "track_started") == 0) {
            prf_track_started_flow(state, sid, in_str(in, "reader_name", "proc-reader"), &s, &exc);
        } else if (strcmp(func, "adopt_local") == 0) {
            ret = prf_adopt_local_flow(state, in_node(in, "proc"), in_str(in, "command", ""), in_node(in, "cwd"),
                                       in_str(in, "task_id", ""), in_str(in, "session_key", ""),
                                       in_str(in, "owner_task_id", ""), in_str(in, "output_so_far", ""),
                                       in_bool(in, "notify_on_complete"), &s, &exc);
        } else if (strcmp(func, "restore_completions") == 0) {
            ret_int = prf_restore_completions_flow(state, &s, &exc);
        } else if (strcmp(func, "signal_kill") == 0) {
            ret = prf_signal_kill_flow(state, sid, in_bool(in, "consume_output"), now, &s, &exc);
        } else {
            json_set(out, "error", json_string("unknown func"));
        }
        dump_state(state);
        json_set(out, "state", state);
    }

    /* ret envelope */
    if (ret_str) { json_set(out, "ret", json_string(ret_str)); free(ret_str); }
    else if (ret) { json_set(out, "ret", ret); }
    else if (strcmp(func, "uncollected_gone") == 0 || strcmp(func, "append_output_if_running") == 0
             || strcmp(func, "mark_exited") == 0 || strcmp(func, "finish_exited") == 0) {
        json_set(out, "ret", json_bool(ret_bool));
    } else if (strcmp(func, "restore_completions") == 0) {
        json_set(out, "ret", json_number(ret_int));
    } else {
        json_set(out, "ret", json_null());
    }
    json_set(out, "exc", exc ? exc : json_null());
    json_set(out, "trace", trace.events ? trace.events : json_array());
    json_free(ctx.counts);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
            free(err); json_free(c); continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        json_t *func_node = json_string(func);
        char *func_s = json_serialize(func_node);
        json_free(func_node);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}
