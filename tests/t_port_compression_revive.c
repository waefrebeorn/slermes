/* t_port_compression_revive.c — harness for the conversation_compression revival.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out = {ret, trace, state}:
 *   ret   — the port's return (NULL -> null; {"__raise__": ...} -> exception)
 *   trace — ordered seam calls {ev, args, ret}
 *   state — the mutated input object minus "io"
 * The echoed "in" is the ORIGINAL input; the oracle re-drives the live Python
 * reference from it under identical scripted seam results.
 *
 * Scripted I/O lives in in.io: seam-name -> result. A result is a plain JSON
 * value (reused), a LIST consumed in call order, or
 * {"__script__": true, "ret": <value>, "raise": {type, message}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 * NOTE: a seam whose live return is a LIST (resolve_context_compression_timeouts,
 * a list-valued getter state) must be scripted via the __script__ wrapper, else
 * the bare array is consumed element-per-call.
 *
 * Gate:
 *   bash tests/run_one_oracle.sh compression_revive oracle_compression_revive.py "" "src/agent/conversation_compression_gaps.o src/agent/conversation_compression_gaps2.o lib/libjson/json.o"
 */
#include "agent/conversation_compression_gaps2.c"
#include "agent/conversation_compression_gaps.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    json_t *io_map;   /* borrowed in.io */
    json_t *cursors;  /* per-seam queue cursors */
} ccr_script_t;

static json_t *ccr_make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    return e;
}

static json_t *ccr_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return ccr_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *ccr_dispatch(ccr_io_t *io, const char *name, json_t *args) {
    (void)args;
    ccr_script_t *sc = (ccr_script_t *)io->ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        return ccr_raise_spec("RuntimeError", msg);
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            return ccr_raise_spec("RuntimeError", msg);
        }
        json_set(sc->cursors, name, json_number(cur + 1));
        return ccr_entry_result(json_get(entry, idx));
    }
    return ccr_entry_result(entry);
}

typedef json_t *(*ccr_fn_t)(ccr_io_t *, json_t *);
typedef struct { const char *func; ccr_fn_t fn; } ccr_case_t;
static const ccr_case_t CCR_CASES[] = {
    { "codex_compaction_cooldown_remaining", codex_compaction_cooldown_remaining },
    { "context_compression_timed_out", context_compression_timed_out },
    { "compression_blocked_transiently", compression_blocked_transiently },
    { "checkpoint_blocked", checkpoint_blocked },
    { "compression_attempt_stalled", compression_attempt_stalled },
    { "deadline_exceeded", deadline_exceeded },
    { "progress_observed", progress_observed },
    { "commit_watermark_fenced", commit_watermark_fenced },
};
#define CCR_CASE_COUNT (sizeof(CCR_CASES) / sizeof(CCR_CASES[0]))

static char *ccr_read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line;
    while ((line = ccr_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const ccr_case_t *tc = NULL;
        for (size_t i = 0; i < CCR_CASE_COUNT; i++) {
            if (strcmp(CCR_CASES[i].func, func) == 0) { tc = &CCR_CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *work = json_copy(in);
            ccr_io_t io;
            ccr_script_t sc;
            io.call = ccr_dispatch;
            io.trace = json_array();
            io.ctx = &sc;
            sc.io_map = json_obj_get(work, "io");
            sc.cursors = json_object();
            json_t *r = tc->fn(&io, work);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(io.trace));
            json_obj_del(work, "io");
            json_set(out, "state", work); /* takes ownership */
            json_free(io.trace);
            json_free(sc.cursors);
        }

        json_t *line_out = json_object();
        json_set_hdr(line_out, "func", func);
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