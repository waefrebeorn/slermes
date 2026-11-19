/*
 * t_port_run_notifications_flow.c — oracle harness for the
 * gateway/run_notifications.py notification-delivery FLOW cores and leaves
 * ported in src/gateway/run_notifications_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires the universal seam to the SCRIPTED results in "in"."io" (recording
 * stubs, one trace event per seam call in call order) and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...], "st": ...}}.
 * tests/oracle_run_notifications_flow.py drives the LIVE Python with the
 * same scripts.
 *
 * Script shapes (mirrored exactly by the oracle):
 *   io[name] = {"const": v}    — v returned on every call
 *   io[name] = {"queue": [..]} — one element consumed per call; exhaustion
 *                                raises RuntimeError("script exhausted for
 *                                <name>"); a missing script raises
 *                                RuntimeError("no script for <name>").
 * A scripted null returns a NULL pointer (Python None), never a json_null
 * node; {"__raise__": cls, "message": msg} models a raised exception.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/run_notifications_flow.c"

typedef struct {
    const json_t *in;
    const json_t *io;
    json_t *cursors;   /* per-name queue call counters */
    json_t *trace;     /* owned array of events */
} hctx_t;

static json_t *raise_spec(const char *cls, const char *msg) {
    json_t *s = json_object();
    json_set(s, "__raise__", json_string(cls));
    json_set(s, "message", json_string(msg));
    return s;
}

static json_t *next_script(hctx_t *c, const char *name) {
    const json_t *s = c->io ? json_obj_get(c->io, name) : NULL;
    char msg[256];
    if (!s) {
        snprintf(msg, sizeof(msg), "no script for %s", name);
        return raise_spec("RuntimeError", msg);
    }
    if (s->type == JSON_OBJECT && json_obj_get(s, "const"))
        return json_copy(json_obj_get(s, "const"));
    if (s->type == JSON_OBJECT && json_obj_get(s, "queue")) {
        const json_t *q = json_obj_get(s, "queue");
        long long cur = (long long)json_get_num(c->cursors, name, 0);
        if (q->type != JSON_ARRAY || cur >= (long long)json_len(q)) {
            snprintf(msg, sizeof(msg), "script exhausted for %s", name);
            return raise_spec("RuntimeError", msg);
        }
        json_set(c->cursors, name, json_number((double)(cur + 1)));
        return json_copy(json_get(q, (size_t)cur));
    }
    snprintf(msg, sizeof(msg), "bad script for %s", name);
    return raise_spec("RuntimeError", msg);
}

/* Universal recording seam: args BORROWED (the port frees them). */
static json_t *h_call(void *ctx, const char *name, const json_t *args) {
    hctx_t *c = ctx;
    json_t *spec = next_script(c, name);
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    json_set(e, "ret", json_copy(spec));
    json_append(c->trace, e);
    if (spec->type == JSON_NULL) { json_free(spec); return NULL; }
    return spec; /* port owns; raise specs parked by rnf_call */
}

static void h_log(void *ctx, int level, const char *fmt, const json_t *args) {
    hctx_t *c = ctx;
    json_t *e = json_object();
    json_set(e, "ev", json_string("log"));
    json_set(e, "level", json_number(level));
    json_set(e, "fmt", json_string(fmt));
    json_set(e, "args", args ? json_copy(args) : json_array());
    json_append(c->trace, e);
}

/* ── dedup-state mirror: fixture lists -> live-shaped state object ── */
static json_t *build_state(const json_t *in_state) {
    json_t *st = json_object();
    const json_t *inf = gflow_obj_get(in_state, "inflight");
    json_set(st, "inflight", inf ? json_copy(inf) : json_array());
    json_t *del = json_object();
    const json_t *dl = gflow_obj_get(in_state, "delivered");
    if (gflow_is_arr(dl)) {
        for (size_t i = 0; i < json_len(dl); i++) {
            char *key = rnf_idkey(json_get(dl, i));
            json_set(del, key, json_null());
            free(key);
        }
    }
    json_set(st, "delivered", del);
    json_set(st, "retention", json_number(json_get_num(in_state, "retention", 100)));
    json_set(st, "lock_held", json_bool(false));
    return st;
}

static json_t *extract_state(const json_t *st) {
    json_t *out = json_object();
    json_t *inf = json_copy(json_obj_get(st, "inflight"));
    if (inf && inf->type == JSON_ARRAY && inf->c.count > 1)
        qsort(inf->c.items, inf->c.count, sizeof(json_t *), rnf_id_cmp);
    json_set(out, "inflight", inf ? inf : json_array());
    json_t *del = json_array();
    const json_t *d = json_obj_get(st, "delivered");
    if (d && d->type == JSON_OBJECT) {
        for (size_t i = 0; i < d->c.count; i++) {
            json_t *id = json_parse(d->c.keys[i], NULL);
            json_append(del, id ? id : json_null());
        }
    }
    json_set(out, "delivered", del);
    const json_t *lh = json_obj_get(st, "lock_held");
    json_set(out, "lock_held", lh ? json_copy(lh) : json_bool(false));
    return out;
}

static const char *opt_str(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* ── dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    hctx_t ctx = { in, json_obj_get(in, "io"), json_object(), json_array() };
    rnf_io_t io = { .ctx = &ctx, .call = h_call, .log = h_log };
    json_set(out, "ret", json_null()); /* reserve envelope position */
    json_t *st = NULL;

    if (strcmp(func, "mark_internal_notification") == 0) {
        json_set(out, "ret", rnf_mark_internal_notification(json_get_str(in, "text", "")));
    } else if (strcmp(func, "raw_process_event_session_id") == 0) {
        json_t *r = rnf_raw_process_event_session_id(json_obj_get(in, "evt"));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "event_route_key") == 0) {
        json_set(out, "ret", rnf_event_route_key(json_obj_get(in, "evt"), json_obj_get(in, "fields")));
    } else if (strcmp(func, "completion_delivery_identity") == 0) {
        json_t *r = rnf_completion_delivery_identity(json_obj_get(in, "evt"));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "served_notice_target_key") == 0) {
        json_t *r = rnf_served_notice_target_key(
            opt_str(in, "profile"), json_get_str(in, "platform_value", ""),
            json_obj_get(in, "chat_id"), json_obj_get(in, "thread_id"), &io);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "refusal_retry_at") == 0) {
        json_t *r = rnf_refusal_retry_at(json_get_num(in, "step", 0), &io);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "held_reason") == 0) {
        json_t *r = rnf_held_reason(json_obj_get(in, "source"),
                                    json_get_str(in, "platform", ""),
                                    json_get_bool(in, "multiplexed", false), &io);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "safe_delivery_transport") == 0) {
        json_t *r = rnf_safe_delivery_transport(
            json_obj_get(in, "platform"), json_obj_get(in, "config"),
            json_obj_get(in, "adapters"), opt_str(in, "profile"), &io);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "completion_identity_seen") == 0) {
        json_t *state = build_state(json_obj_get(in, "state"));
        bool r = rnf_completion_identity_seen(state, json_obj_get(in, "identity"),
                                              json_get_bool(in, "claim", false));
        json_set(out, "ret", json_bool(r));
        st = extract_state(state);
        json_free(state);
    } else if (strcmp(func, "mark_completions_delivered_locked") == 0) {
        json_t *state = build_state(json_obj_get(in, "state"));
        rnf_mark_completions_delivered_locked(state, json_obj_get(in, "identities"));
        st = extract_state(state);
        json_free(state);
    } else if (strcmp(func, "record_coalesced_completion_siblings") == 0) {
        json_t *state = build_state(json_obj_get(in, "state"));
        rnf_record_coalesced_completion_siblings(state, json_obj_get(in, "events"));
        st = extract_state(state);
        json_free(state);
    } else if (strcmp(func, "requeue_refused_watch_event") == 0) {
        json_t *evt = json_copy(json_obj_get(in, "evt"));
        if (!evt) evt = json_object();
        rnf_requeue_refused_watch_event(evt, &io);
        st = json_object();
        json_set(st, "evt", evt);
    } else if (strcmp(func, "back_off_refused_completions") == 0) {
        json_t *group = json_copy(json_obj_get(in, "group"));
        if (!group) group = json_array();
        rnf_back_off_refused_completions(group, &io);
        st = json_object();
        json_set(st, "group", group);
    } else if (strcmp(func, "settle_durable_claim") == 0) {
        rnf_settle_durable_claim(json_get_str(in, "kind", ""),
                                 json_get_str(in, "delegation_id", ""),
                                 json_get_str(in, "claim_id", ""), &io);
    } else if (strcmp(func, "watch_notification_metadata") == 0) {
        json_t *r = rnf_watch_notification_metadata(json_obj_get(in, "evt"),
                                                    json_obj_get(in, "source"), &io);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "drain_watch_notifications") == 0) {
        rnf_drain_watch_notifications(json_get_str(in, "queue_id", "q"), &io);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    if (rnf_pending) {
        json_set(out, "ret", rnf_pending);
        rnf_pending = NULL;
    }
    json_set(out, "trace", ctx.trace);
    if (st) json_set(out, "st", st);
    json_free(ctx.cursors);
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
            free(err);
            json_free(c);
            continue;
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