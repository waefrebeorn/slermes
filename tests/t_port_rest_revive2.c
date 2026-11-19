/* t_port_rest_revive2.c — harness for the rest group-2 deep revival ports.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out = {ret, trace, state}:
 *   ret   — the port's return (NULL -> null; {"__raise__": ...} -> exception)
 *   trace — ordered seam calls {ev, args, ret}
 *   state — the mutated input object minus "io"
 * The echoed "in" is the ORIGINAL input; the oracle re-drives the live
 * Python reference from it under identical scripted seam results.
 *
 * Scripted I/O lives in in.io: seam-name -> result. A result is a plain
 * JSON value (reused), a LIST consumed in call order, or
 * {"__script__": true, "ret": <value>, "raise": {type, message}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * Gate (explicit objs; the harness #includes the port sources, so only
 * libjson is linked — both gap files are libjson-only):
 *   bash tests/run_one_oracle.sh rest_revive2 oracle_rest_revive2.py "" "lib/libjson/json.o"
 */
#include "tools/cronjob_tools_gaps.c"
#include "hermes_cli/cron_gaps.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    json_t *io_map;   /* borrowed in.io */
    json_t *cursors;  /* per-seam queue cursors */
} r2h_script_t;

static json_t *r2h_make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    return e;
}

static json_t *r2h_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return r2h_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *r2h_raise_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message);
    return e;
}

/* Script lookup shared by every per-file dispatcher. */
static json_t *r2h_dispatch_impl(void *ctx, const char *name, json_t *args) {
    (void)args;
    r2h_script_t *sc = (r2h_script_t *)ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        return r2h_raise_spec("RuntimeError", msg);
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            return r2h_raise_spec("RuntimeError", msg);
        }
        json_set(sc->cursors, name, json_number(cur + 1));
        return r2h_entry_result(json_get(entry, idx));
    }
    return r2h_entry_result(entry);
}

/* One type-correct dispatcher per port io type. */
static json_t *cj_disp(cj_io_t *io, const char *name, json_t *args) {
    return r2h_dispatch_impl(io->ctx, name, args);
}
static json_t *hc_disp(hc_io_t *io, const char *name, json_t *args) {
    return r2h_dispatch_impl(io->ctx, name, args);
}

/* One wrapper per case; pure ports take void* io and ignore it. */
#define R2_WRAP_CJ(NAME, FN) \
    static json_t *wrap_##NAME(json_t *trace, void *sc, json_t *work) { \
        cj_io_t io; \
        io.call = cj_disp; \
        io.trace = trace; \
        io.ctx = sc; \
        return FN(&io, work); \
    }
#define R2_WRAP_HC(NAME, FN) \
    static json_t *wrap_##NAME(json_t *trace, void *sc, json_t *work) { \
        hc_io_t io; \
        io.call = hc_disp; \
        io.trace = trace; \
        io.ctx = sc; \
        return FN(&io, work); \
    }
#define R2_WRAP_PURE(NAME, FN) \
    static json_t *wrap_##NAME(json_t *trace, void *sc, json_t *work) { \
        (void)trace; (void)sc; \
        return FN(NULL, work); \
    }

R2_WRAP_CJ(run_claimed_job, run_claimed_job)
R2_WRAP_CJ(try_dispatch_background_run, try_dispatch_background_run)
R2_WRAP_PURE(mode_guidance_notes, mode_guidance_notes)
R2_WRAP_CJ(resolve_cron_context_deliver, resolve_cron_context_deliver)
R2_WRAP_CJ(forward_relay_fronted_run, forward_relay_fronted_run)
R2_WRAP_CJ(latest_job_output_excerpt, latest_job_output_excerpt)
R2_WRAP_PURE(manual_run_delivery_note, manual_run_delivery_note)
R2_WRAP_CJ(_cronjob_handler, _cronjob_handler)
R2_WRAP_PURE(split_monitor_arg, split_monitor_arg)
R2_WRAP_CJ(gateway_liveness_notice, gateway_liveness_notice)
R2_WRAP_HC(cron_doctor_issues_for_job, cron_doctor_issues_for_job)
R2_WRAP_HC(dispatch_display, dispatch_display)
R2_WRAP_HC(script_health_issue, script_health_issue)
R2_WRAP_PURE(format_lateness, format_lateness)
R2_WRAP_HC(next_run_overdue_issue, next_run_overdue_issue)
R2_WRAP_HC(scripts_dir_for_cron, scripts_dir_for_cron)

typedef json_t *(*r2h_fn_t)(json_t *, void *, json_t *);
typedef struct { const char *func; r2h_fn_t fn; } r2h_case_t;

static const r2h_case_t R2H_CASES[] = {
    { "run_claimed_job", wrap_run_claimed_job },
    { "try_dispatch_background_run", wrap_try_dispatch_background_run },
    { "mode_guidance_notes", wrap_mode_guidance_notes },
    { "resolve_cron_context_deliver", wrap_resolve_cron_context_deliver },
    { "forward_relay_fronted_run", wrap_forward_relay_fronted_run },
    { "latest_job_output_excerpt", wrap_latest_job_output_excerpt },
    { "manual_run_delivery_note", wrap_manual_run_delivery_note },
    { "_cronjob_handler", wrap__cronjob_handler },
    { "split_monitor_arg", wrap_split_monitor_arg },
    { "gateway_liveness_notice", wrap_gateway_liveness_notice },
    { "cron_doctor_issues_for_job", wrap_cron_doctor_issues_for_job },
    { "dispatch_display", wrap_dispatch_display },
    { "script_health_issue", wrap_script_health_issue },
    { "format_lateness", wrap_format_lateness },
    { "next_run_overdue_issue", wrap_next_run_overdue_issue },
    { "scripts_dir_for_cron", wrap_scripts_dir_for_cron },
};
#define R2H_CASE_COUNT (sizeof(R2H_CASES) / sizeof(R2H_CASES[0]))

static char *r2h_read_line(FILE *f) {
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
    while ((line = r2h_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const r2h_case_t *tc = NULL;
        for (size_t i = 0; i < R2H_CASE_COUNT; i++) {
            if (strcmp(R2H_CASES[i].func, func) == 0) { tc = &R2H_CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *work = json_copy(in);
            r2h_script_t sc;
            sc.io_map = json_obj_get(work, "io");
            sc.cursors = json_object();
            json_t *trace = json_array();
            json_t *r = tc->fn(trace, &sc, work);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(trace));
            json_obj_del(work, "io");
            json_set(out, "state", work); /* takes ownership */
            json_free(trace);
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
