/* t_port_context_flow.c — harness for the compaction FLOW-core port.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func": <name>, "out": {...}} where out captures the
 * decision SEQUENCE (ordered seam-call names in "trace"), the final return
 * ("ret"), the mutated attribute state ("state"), pin state ("pin") and/or an
 * ordered result sequence ("seq").
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is
 * {"ret": <value>, "state": {<attr effects>}, "raise": {"type","message"}}
 * (keys optional). A result may be a LIST consumed in call order (a queue);
 * a single object is reused for every call. The dispatcher applies "state"
 * effects onto the live attribute object and models "raise" as an exception,
 * exactly mirroring the oracle's recording stubs fed the same scripts.
 */
#include "hermes_json.h"
#include "context_compressor_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ── scripted-I/O backend ──────────────────────────────────────────────── */
typedef struct {
    json_t *io;        /* in.io map */
    json_t *cursors;   /* object name -> consumed count (for queue scripts) */
    json_t *verdict;   /* in.verdict {reason,transport} for classify_api_error */
} script_ctx_t;

static json_t *script_next(script_ctx_t *sc, const char *name) {
    if (!sc || !sc->io) return NULL;
    json_t *entry = json_obj_get(sc->io, name);
    if (!entry) return NULL;
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        size_t n = json_len(entry);
        if (idx >= n) return NULL; /* exhausted */
        json_set(sc->cursors, name, json_number(cur + 1));
        return json_get(entry, idx);
    }
    return entry; /* single result reused */
}

/* Apply {"state":{...}} effects onto the attribute object. */
static void apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}

/* Capture decision-relevant seam args into state (mirrors oracle captures). */
static void capture_decisions(json_t *state, const char *name, json_t *args) {
    if (!args || args->type != JSON_OBJECT) return;
    if (strcmp(name, "_record_compression_failure_cooldown") == 0) {
        json_t *s = json_obj_get(args, "seconds");
        json_t *e = json_obj_get(args, "error");
        if (s) json_set(state, "_dec_cooldown_seconds", json_copy(s));
        if (e) json_set(state, "_dec_cooldown_error", json_copy(e));
    } else if (strcmp(name, "_fallback_to_main_for_compression") == 0) {
        json_t *r = json_obj_get(args, "reason");
        json_t *m = json_obj_get(args, "failed_model");
        if (r) json_set(state, "_dec_fb_reason", json_copy(r));
        if (m) json_set(state, "_dec_fb_model", json_copy(m));
    } else if (strcmp(name, "_structural_no_op_result") == 0) {
        json_t *c = json_obj_get(args, "failure_class");
        if (c) json_set(state, "_dec_noop_class", json_copy(c));
    } else if (strcmp(name, "_generate_summary") == 0) {
        json_t *b = json_obj_get(args, "bypass_cooldown");
        if (b) json_set(state, "_dec_retry_bypass", json_copy(b));
    }
}

static json_t *dispatch(cc_flow_io_t *io, const char *name, json_t *args) {
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    capture_decisions(io->state, name, args);
    json_t *result = script_next(sc, name);
    apply_state_effects(io->state, result);
    if (result && result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) {
            json_t *exc = json_object();
            json_set(exc, "__raise__", json_copy(json_obj_get(raise, "type")));
            json_t *msg = json_obj_get(raise, "message");
            json_set(exc, "message", msg ? json_copy(msg) : json_string(""));
            return exc;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_null();
}

/* ── exc descriptor parsing ────────────────────────────────────────────── */
static cc_flow_exc_t parse_exc(const json_t *in) {
    cc_flow_exc_t e;
    memset(&e, 0, sizeof(e));
    json_t *ex = json_obj_get(in, "exc");
    e.type = json_get_str(ex, "type", "RuntimeError");
    e.message = json_get_str(ex, "message", "");
    e.has_status = json_has(ex, "status_code") &&
                   json_obj_get(ex, "status_code") &&
                   json_obj_get(ex, "status_code")->type != JSON_NULL;
    e.status_code = (int)json_get_num(ex, "status_code", -1);
    json_t *code = json_obj_get(ex, "code");
    e.code = (code && code->type == JSON_STRING) ? code->str_val : NULL;
    e.unscoped_secret = json_get_bool(ex, "unscoped_secret", false);
    return e;
}

static cc_flow_verdict_t parse_verdict(const json_t *in) {
    cc_flow_verdict_t v;
    json_t *vd = json_obj_get(in, "verdict");
    v.reason = json_get_str(vd, "reason", NULL);
    v.transport = json_get_bool(vd, "transport", false);
    return v;
}

/* ── io construction ───────────────────────────────────────────────────── */
static void io_init(cc_flow_io_t *io, script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = dispatch;
    io->trace = json_array();
    io->state = state;
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
    sc->verdict = json_obj_get(in, "verdict");
}

static json_t *state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

/* Build the common out object. */
static json_t *make_out(cc_flow_io_t *io, json_t *ret, json_t *pin, json_t *seq) {
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", io->trace ? json_copy(io->trace) : json_array());
    json_set(out, "state", io->state ? json_copy(io->state) : json_object());
    if (pin) json_set(out, "pin", json_copy(pin));
    if (seq) json_set(out, "seq", seq);
    return out;
}

/* ── per-func drivers ──────────────────────────────────────────────────── */

static json_t *drive_pin(json_t *in) {
    json_t *pin = json_obj_get(in, "pin");
    pin = (pin && pin->type == JSON_OBJECT) ? json_copy(pin) : json_object();
    if (!json_has(pin, "route")) json_set(pin, "route", json_null());
    json_t *ops = json_obj_get(in, "ops");
    json_t *seq = json_array();
    size_t n = ops ? json_len(ops) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *op = json_get(ops, i);
        const char *k = json_get_str(op, "op", "");
        json_t *r = NULL;
        if (strcmp(k, "set") == 0) {
            cc_flow_pin_summary_route_set(pin, json_obj_get(op, "route"));
            r = json_null();
        } else if (strcmp(k, "take") == 0) {
            r = cc_flow_take_pinned_summary_route(pin);
        } else if (strcmp(k, "take_det") == 0) {
            r = json_bool(cc_flow_take_deterministic_summary_pin(pin));
        } else if (strcmp(k, "kwargs") == 0) {
            r = cc_flow_pinned_summary_call_kwargs(pin);
        } else {
            r = json_null();
        }
        json_append(seq, r ? r : json_null());
    }
    json_t *out = json_object();
    json_set(out, "seq", seq);
    json_set(out, "pin", json_copy(pin));
    json_free(pin);
    return out;
}

static json_t *drive_next_timeout(json_t *in) {
    json_t *state = state_from(in);
    json_t *calls = json_obj_get(in, "calls");
    json_t *seq = json_array();
    size_t n = calls ? json_len(calls) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *c = json_get(calls, i);
        const char *counter = (c && c->type == JSON_STRING) ? c->str_val : "_consecutive_timeout_failures";
        int rung = cc_flow_next_timeout_cooldown(state, counter);
        json_append(seq, json_number((double)rung));
    }
    json_t *out = json_object();
    json_set(out, "seq", seq);
    json_set(out, "state", state);
    return out;
}

static json_t *drive_walk(json_t *in) {
    cc_flow_io_t io; script_ctx_t sc;
    json_t *state = json_object();
    io_init(&io, &sc, in, state);
    json_t *messages = json_obj_get(in, "messages");
    long head_end = (long)json_get_num(in, "head_end", 0);
    long ceiling = (long)json_get_num(in, "ceiling", 0);
    long min_tail = (long)json_get_num(in, "min_tail", 0);
    bool cut_at_break = json_get_bool(in, "cut_at_break", false);
    json_t *prices = json_obj_get(in, "prices");
    long *pr = NULL;
    bool use_seam = true;
    if (prices && prices->type == JSON_ARRAY) {
        size_t pn = json_len(prices);
        pr = malloc(sizeof(long) * (pn + 1));
        for (size_t i = 0; i < pn; i++) {
            json_t *pv = json_get(prices, i);
            pr[i] = (pv && pv->type == JSON_NUMBER) ? (long)pv->num_val : 0;
        }
        use_seam = false;
    }
    json_t *ret = cc_flow_walk_tail_budget(&io, messages, head_end, ceiling, min_tail, cut_at_break, pr, use_seam);
    free(pr);
    json_t *out = make_out(&io, ret, NULL, NULL);
    json_free(sc.cursors); json_free(state);
    return out;
}

/* ── main loop ─────────────────────────────────────────────────────────── */
static char *read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    char *line;
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        if (strcmp(func, "pin") == 0) {
            out = drive_pin(in);
        } else if (strcmp(func, "next_timeout") == 0) {
            out = drive_next_timeout(in);
        } else if (strcmp(func, "classify") == 0) {
            cc_flow_exc_t e = parse_exc(in);
            cc_flow_verdict_t v = parse_verdict(in);
            json_t *ret = cc_flow_classify_summary_failure(&e, &v);
            out = json_object(); json_set(out, "ret", ret);
        } else if (strcmp(func, "access_or_quota") == 0) {
            cc_flow_exc_t e = parse_exc(in);
            cc_flow_verdict_t v = parse_verdict(in);
            bool r = cc_flow_is_summary_access_or_quota_error(&e, &v);
            out = json_object(); json_set(out, "ret", json_bool(r));
        } else if (strcmp(func, "short_error") == 0) {
            const char *se = json_get_str(in, "str_e", "");
            const char *cn = json_get_str(in, "class_name", "Exception");
            int limit = (int)json_get_num(in, "limit", 220);
            char *r = cc_flow_short_error_text(se, cn, limit);
            out = json_object(); json_set(out, "ret", json_string(r ? r : "")); free(r);
        } else if (strcmp(func, "block_reason") == 0) {
            json_t *state = state_from(in);
            double now = json_get_num(in, "now", 0);
            bool ic = json_get_bool(in, "ignore_cooldown", false);
            json_t *ret = cc_flow_compression_block_reason(state, now, ic);
            out = json_object(); json_set(out, "ret", ret); json_set(out, "state", state);
        } else if (strcmp(func, "placement") == 0) {
            json_t *compressed = json_obj_get(in, "compressed");
            json_t *tail = json_obj_get(in, "tail_messages");
            long cs = (long)json_get_num(in, "compress_start", 0);
            json_t *ret = cc_flow_summary_placement(compressed, tail, cs);
            out = json_object(); json_set(out, "ret", ret);
        } else if (strcmp(func, "walk_tail") == 0) {
            out = drive_walk(in);
        } else if (strcmp(func, "should_compress_info") == 0) {
            cc_flow_io_t io; script_ctx_t sc; json_t *state = state_from(in);
            io_init(&io, &sc, in, state);
            bool hpt = json_has(in, "prompt_tokens") && json_obj_get(in, "prompt_tokens") &&
                       json_obj_get(in, "prompt_tokens")->type != JSON_NULL;
            long pt = (long)json_get_num(in, "prompt_tokens", 0);
            double now = json_get_num(in, "now", 0);
            double wall = json_get_num(in, "wall", 0);
            json_t *ret = cc_flow_should_compress_info(&io, state, hpt, pt, now, wall);
            out = make_out(&io, ret, NULL, NULL);
            json_free(sc.cursors);
        } else if (strcmp(func, "blocked_locally") == 0 || strcmp(func, "blocked") == 0) {
            cc_flow_io_t io; script_ctx_t sc; json_t *state = state_from(in);
            io_init(&io, &sc, in, state);
            double now = json_get_num(in, "now", 0);
            double wall = json_get_num(in, "wall", 0);
            bool ic = json_get_bool(in, "ignore_cooldown", false);
            bool r = (strcmp(func, "blocked") == 0)
                ? cc_flow_automatic_compression_blocked(&io, state, now, wall, ic)
                : cc_flow_automatic_compression_blocked_locally(&io, state, now, wall, ic);
            out = make_out(&io, json_bool(r), NULL, NULL);
            json_free(sc.cursors);
        } else if (strcmp(func, "fallback_to_main") == 0) {
            cc_flow_io_t io; script_ctx_t sc; json_t *state = state_from(in);
            io_init(&io, &sc, in, state);
            cc_flow_exc_t e = parse_exc(in);
            const char *reason = json_get_str(in, "reason", "");
            const char *fm = json_get_str(in, "failed_model", NULL);
            cc_flow_fallback_to_main_for_compression(&io, state, &e, reason, fm);
            out = make_out(&io, json_null(), NULL, NULL);
            json_free(sc.cursors);
        } else if (strcmp(func, "on_summary_failure") == 0) {
            cc_flow_io_t io; script_ctx_t sc; json_t *state = state_from(in);
            io_init(&io, &sc, in, state);
            cc_flow_exc_t e = parse_exc(in);
            cc_flow_verdict_t v = parse_verdict(in);
            json_t *turns = json_obj_get(in, "turns");
            const char *ft = json_get_str(in, "focus_topic", NULL);
            const char *mc = json_get_str(in, "memory_context", "");
            json_t *ret = cc_flow_on_summary_failure(&io, state, &e, &v, turns, ft, mc);
            out = make_out(&io, ret, NULL, NULL);
            json_free(sc.cursors);
        } else if (strcmp(func, "generate_summary") == 0) {
            cc_flow_io_t io; script_ctx_t sc; json_t *state = state_from(in);
            io_init(&io, &sc, in, state);
            json_t *turns = json_obj_get(in, "turns");
            const char *ft = json_get_str(in, "focus_topic", NULL);
            const char *mc = json_get_str(in, "memory_context", "");
            bool bc = json_get_bool(in, "bypass_cooldown", false);
            double now = json_get_num(in, "now", 0);
            json_t *ret = cc_flow_generate_summary(&io, state, turns, ft, mc, bc, now);
            out = make_out(&io, ret, NULL, NULL);
            json_free(sc.cursors);
        } else if (strcmp(func, "compress") == 0) {
            cc_flow_io_t io; script_ctx_t sc; json_t *state = state_from(in);
            io_init(&io, &sc, in, state);
            json_t *messages = json_obj_get(in, "messages");
            bool hct = json_has(in, "current_tokens") && json_obj_get(in, "current_tokens") &&
                       json_obj_get(in, "current_tokens")->type != JSON_NULL;
            long ct = (long)json_get_num(in, "current_tokens", 0);
            const char *ft = json_get_str(in, "focus_topic", NULL);
            bool force = json_get_bool(in, "force", false);
            const char *mc = json_get_str(in, "memory_context", "");
            bool bc = json_get_bool(in, "bypass_cooldown", false);
            json_t *ret = cc_flow_compress(&io, state, messages, hct, ct, ft, force, mc, bc);
            out = make_out(&io, ret, NULL, NULL);
            json_free(sc.cursors);
        } else {
            out = json_object();
            json_set(out, "error", json_string("unknown func"));
        }

        json_t *line_out = json_object();
        json_set(line_out, "func", json_string(func));
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out ? out : json_null());
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(rec);
    }
    return 0;
}