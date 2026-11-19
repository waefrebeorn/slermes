/*
 * t_port_cli_gaps_revive1.c -- differential harness for the cli_gaps.c
 * revival group 1 ports. Reads JSONL cases on stdin, each
 *   {"func": <slot>, "in": {...}}
 * wires a universal recording seam driven by in["io"] scripts, calls the
 * C port, and echoes one line per case:
 *   {"func":..., "in":{...}, "out":{"ret":...,"trace":[...],"state":{...},"raise":...}}
 * tests/oracle_cli_gaps_revive1.py replays the SAME scripts through the
 * live Python and compares ordered trace + ret + state + raise.
 *
 * Self-contained: #includes the port .c. Gate (explicit objs):
 *   bash tests/run_one_oracle.sh cli_gaps_revive1 oracle_cli_gaps_revive1.py "" \
 *        "src/cli/cli_gaps.o lib/libjson/json.o"
 */
#include "cli/cli_gaps.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal recording seam ── */
typedef struct {
    const json_t *io;     /* per-name scripts */
    json_t *trace;        /* ordered events */
    char *names[128];
    size_t idx[128];
    size_t nn;
} cg1_rec;

static size_t rec_slot(cg1_rec *r, const char *name) {
    for (size_t i = 0; i < r->nn; i++)
        if (strcmp(r->names[i], name) == 0) return i;
    size_t k = r->nn++;
    r->names[k] = strdup(name);
    r->idx[k] = 0;
    return k;
}

static json_t *rec_call(void *ctx, const char *name, const json_t *args) {
    cg1_rec *r = (cg1_rec *)ctx;
    json_t *script = r->io ? json_obj_get(r->io, name) : NULL;
    json_t *value;
    if (!script) {
        /* fail closed: an unscripted seam is a harness/fixture bug */
        value = json_object();
        json_set(value, "__raise__", json_string("MissingScript"));
        json_set(value, "message", json_string(name));
    } else if (script->type == JSON_OBJECT && json_obj_get(script, "__seq__")) {
        json_t *seq = json_obj_get(script, "__seq__");
        size_t k = rec_slot(r, name);
        size_t n = json_len(seq);
        size_t use = r->idx[k] < n ? r->idx[k] : (n ? n - 1 : 0);
        r->idx[k]++;
        value = n ? json_copy(json_get(seq, use)) : json_null();
    } else {
        value = json_copy(script);
    }
    /* record event {ev,args,ret} */
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", json_copy(value));
    json_append(r->trace, ev);
    /* return: NULL pointer for None (NOT a json_null node), copy otherwise */
    if (!value || value->type == JSON_NULL) { json_free(value); return NULL; }
    return value;
}

/* ── dispatch: fixture slot name -> C port call ── */
static json_t *dispatch(const char *f, const json_t *in, const cg1_io *io, json_t *st, cg1_exc *e) {
#define D(name, ...) if (strcmp(f, name) == 0) return (__VA_ARGS__)
    D("_is_pausable_gateway", hermes_cli_scan_venv_blockers_is_pausable_gateway(in, io, st, e));
    D("probe_port", hermes_cli_local_runtime_detect_probe_port(in, io, st, e));
    D("pairing_command", hermes_cli_pairing_pairing_command(in, io, st, e));
    D("extract_compress_flags", hermes_cli_partial_compress_extract_compress_flags(in, io, st, e));
    D("summarize_compress_preview", hermes_cli_partial_compress_summarize_compress_preview(in, io, st, e));
    D("is_valid", tools_browser_supervisor_capture_is_valid(in, io, st, e));
    D("_invalid", tools_browser_supervisor_capture_invalid(in, io, st, e));
    D("_own_launcher", tools_bot_desktop_idle_watch_own_launcher(in, io, st, e));
    D("kanban_toolset_requested", tools_kanban_toolset_context_kanban_toolset_requested(in, io, st, e));
    D("publish", publish(in, io, st, e));
    D("set_accounting_context", set_accounting_context(in, io, st, e));
    D("reset_accounting_context", reset_accounting_context(in, io, st, e));
    D("error_class", error_class(in, io, st, e));
    D("record_compression_regions", record_compression_regions(in, io, st, e));
    D("note_aux_route_fallback", note_aux_route_fallback(in, io, st, e));
    D("iter_configured_targets", iter_configured_targets(in, io, st, e));
    D("redirect_request", redirect_request(in, io, st, e));
    D("cache_path_base", cache_path_base(in, io, st, e));
    D("default_cwd", default_cwd(in, io, st, e));
    D("check_requirements", check_requirements(in, io, st, e));
    D("setup_instructions", setup_instructions(in, io, st, e));
    D("post_setup", post_setup(in, io, st, e));
    D("doctor_checks", doctor_checks(in, io, st, e));
    D("blocked", blocked(in, io, st, e));
    D("live_dm_allow_from", live_dm_allow_from(in, io, st, e));
    D("checkout", checkout(in, io, st, e));
    D("close_all", close_all(in, io, st, e));
    D("_timer", _timer(in, io, st, e));
    D("_bearer", _bearer(in, io, st, e));
#undef D
    return NULL;
}

static void run_case(const char *func, json_t *in) {
    cg1_rec rec;
    memset(&rec, 0, sizeof rec);
    rec.io = cg1_get(in, "io");
    rec.trace = json_array();
    cg1_io io = { rec_call, &rec };
    json_t *stin = cg1_get(in, "state");
    json_t *state = stin ? json_copy(stin) : json_object();
    cg1_exc exc; exc.kind = NULL; exc.msg = NULL;

    json_t *ret = dispatch(func, in, &io, state, &exc);

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", rec.trace);
    json_set(out, "state", state);
    if (exc.kind) {
        json_t *rz = json_object();
        json_set(rz, "kind", json_string(exc.kind));
        json_set(rz, "message", json_string(exc.msg ? exc.msg : ""));
        json_set(out, "raise", rz);
    } else {
        json_set(out, "raise", json_null());
    }
    char *ins = json_serialize(in);
    char *outs = json_serialize(out);
    printf("{\"func\":\"%s\",\"in\":%s,\"out\":%s}\n", func, ins ? ins : "{}", outs ? outs : "{}");
    free(ins); free(outs);
    cg1_exc_clear(&exc);
    for (size_t i = 0; i < rec.nn; i++) free(rec.names[i]);
    json_free(out); /* frees ret/trace/state/rz children */
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    static char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        json_t *rec = json_parse(line, NULL);
        if (!rec) continue;
        const char *func = cg1_str(rec, "func", "");
        json_t *in = cg1_get(rec, "in");
        json_t *inc = in ? json_copy(in) : json_object();
        run_case(func, inc);
        json_free(inc);
        json_free(rec);
    }
    return 0;
}
