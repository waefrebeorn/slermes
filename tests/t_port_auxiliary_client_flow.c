/* t_port_auxiliary_client_flow.c — harness for the aux-call FLOW-core port.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam calls {"name","args"} in "trace"), the final return
 * ("ret"), and the relay-context state ("relay_context").
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either a
 * plain JSON value (returned verbatim) or {"__script__": true, "ret": <value>,
 * "raise": {type,message,status_code?,exc_id?,classified?,base_only?}}. A
 * result may be a LIST consumed in call order (a queue); a single value is
 * reused for every call. The dispatcher mirrors the oracle's recording stubs
 * fed the same scripts.
 *
 * Special seam: rate_limit_retry_rungs (a live yield-from generator). Its
 * script {"__script__": true, "steps": [...], "resp": ..., "failure": ...}
 * performs each sub-step through perform/time.sleep (traced on both sides),
 * then returns {"resp","failure"} — exactly what the live driver feeds back.
 *
 * Data model: clients are referenced by ID (in.clients registry id -> attrs);
 * exceptions are descriptors {"__raise__",message,exc_id,status_code?,
 * classified?,base_only?}; failures {"error","verdict","rejects_parameter",
 * "client"} materialize to {"__failure__":true,...}. See auxiliary_client_flow.h.
 */
#include "hermes_json.h"
#include "auxiliary_client_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ── scripted-I/O backend ──────────────────────────────────────────────── */
typedef struct {
    json_t *io_map;    /* in.io map */
    json_t *cursors;   /* object name -> consumed count (queue scripts) */
} script_ctx_t;

static json_t *script_next(script_ctx_t *sc, const char *name) {
    if (!sc || !sc->io_map) return NULL;
    json_t *entry = json_obj_get(sc->io_map, name);
    if (!entry) return NULL;
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        size_t n = json_len(entry);
        if (idx >= n) return NULL;   /* exhausted */
        json_set(sc->cursors, name, json_number(cur + 1));
        return json_get(entry, idx);
    }
    return entry;   /* single result reused */
}

/* local copy helper (the port's is file-static) */
static json_t *copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* ── exception / failure materialization ───────────────────────────────── */
static json_t *make_failure(json_t *spec);

static json_t *make_exc(json_t *spec) {
    if (!spec || spec->type == JSON_NULL) return json_null();
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    json_set_hdr(e, "exc_id", json_get_str(spec, "exc_id", ""));
    json_t *st = json_obj_get(spec, "status_code");
    if (st && st->type == JSON_NUMBER) json_set(e, "status_code", json_copy(st));
    json_t *classified = json_obj_get(spec, "classified");
    if (classified && classified->type == JSON_OBJECT)
        json_set(e, "classified", make_failure(classified));
    if (json_get_bool(spec, "base_only", false)) json_set(e, "base_only", json_bool(true));
    return e;
}

static json_t *make_failure(json_t *spec) {
    if (!spec || spec->type == JSON_NULL) return json_null();
    json_t *f = json_object();
    json_set(f, "__failure__", json_bool(true));
    json_set(f, "error", make_exc(json_obj_get(spec, "error")));
    json_t *verdict = json_obj_get(spec, "verdict");
    json_set(f, "verdict", verdict ? json_copy(verdict) : json_object());
    json_set(f, "rejects_parameter", json_bool(json_get_bool(spec, "rejects_parameter", false)));
    json_t *client = json_obj_get(spec, "client");
    json_set(f, "client", client ? json_copy(client) : json_null());
    return f;
}

/* first_err may be a Failure spec (has "error") or a bare exception spec */
static json_t *make_failure_or_exc(json_t *spec) {
    if (!spec || spec->type == JSON_NULL) return json_null();
    if (spec->type == JSON_OBJECT && json_obj_get(spec, "error")) return make_failure(spec);
    return make_exc(spec);
}

/* ── route normalization (all _LadderRoute fields present) ─────────────── */
static json_t *normalize_route(json_t *r) {
    json_t *out = r ? json_copy(r) : json_object();
    if (out->type != JSON_OBJECT) { json_free(out); out = json_object(); }
    static const char *nullable[] = {
        "client", "task", "resolved_provider", "resolved_model", "resolved_base_url",
        "resolved_api_key", "resolved_api_mode", "final_model", "main_runtime",
        "route_info", "timeout", "base_info",
    };
    for (size_t i = 0; i < sizeof(nullable) / sizeof(char *); i++)
        if (!json_has(out, nullable[i])) json_set(out, nullable[i], json_null());
    if (!json_has(out, "tag")) json_set_hdr(out, "tag", "");
    if (!json_has(out, "async_mode")) json_set(out, "async_mode", json_bool(false));
    return out;
}

/* relay context: materialize the nested attempt's error/failure */
static json_t *make_relay_context(json_t *spec) {
    if (!spec || spec->type != JSON_OBJECT) return json_null();
    json_t *ctx = json_copy(spec);
    json_t *attempt = json_obj_get(ctx, "attempt");
    if (attempt && attempt->type == JSON_OBJECT) {
        json_t *ma = json_object();
        json_set(ma, "error", make_exc(json_obj_get(attempt, "error")));
        json_set(ma, "provider", copy_or_null(json_obj_get(attempt, "provider")));
        json_set(ma, "model", copy_or_null(json_obj_get(attempt, "model")));
        json_set(ma, "failure", make_failure(json_obj_get(attempt, "failure")));
        json_set(ma, "attempt", copy_or_null(json_obj_get(attempt, "attempt")));
        json_set(ctx, "attempt", ma);
    }
    return ctx;
}

/* ── dispatcher ────────────────────────────────────────────────────────── */
static json_t *dispatch(acf_io_t *io, const char *name, json_t *args) {
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    json_t *entry = script_next(sc, name);
    /* rate_limit_retry_rungs: drive the scripted sub-steps, then return the
     * (resp, failure) pair the live yield-from feeds back into the ladder. */
    if (strcmp(name, "rate_limit_retry_rungs") == 0) {
        if (!entry || entry->type != JSON_OBJECT) return json_null();
        json_t *steps = json_obj_get(entry, "steps");
        if (steps && steps->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(steps); i++) {
                json_t *step = json_get(steps, i);
                const char *kind = json_get_str(step, "kind", "");
                if (strcmp(kind, "wait") == 0) {
                    json_t *wa = json_object();
                    json_t *sargs = json_obj_get(step, "args");
                    json_set(wa, "seconds", copy_or_null(
                        (sargs && sargs->type == JSON_ARRAY && json_len(sargs) > 0) ? json_get(sargs, 0) : NULL));
                    json_t *wr = acf_call(io, "time.sleep", wa);
                    json_free(wa);
                    json_free(wr);
                } else {
                    json_t *pr = acf_call(io, "perform", step);
                    json_free(pr);
                }
            }
        }
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return make_exc(raise);
        json_t *out = json_object();
        json_set(out, "resp", copy_or_null(json_obj_get(entry, "resp")));
        json_set(out, "failure", make_failure(json_obj_get(entry, "failure")));
        return out;
    }
    if (!entry) return json_null();   /* exhausted script */
    if (entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    /* perform on an "attempt" step returns (resp, failure, repairs): the
     * failure spec materializes to a Failure, mirroring the oracle stub. */
    if (strcmp(name, "perform") == 0 && args && json_get_str(args, "kind", NULL)
        && strcmp(json_get_str(args, "kind", ""), "attempt") == 0
        && entry->type == JSON_OBJECT) {
        json_t *out = json_copy(entry);
        json_set(out, "failure", make_failure(json_obj_get(entry, "failure")));
        return out;
    }
    return json_copy(entry);
}

/* ── io construction ───────────────────────────────────────────────────── */
static void io_init(acf_io_t *io, script_ctx_t *sc, json_t *in) {
    io->call = dispatch;
    io->trace = json_array();
    io->clients = json_obj_get(in, "clients");   /* borrowed from the case */
    io->relay_context = make_relay_context(json_obj_get(in, "relay_context"));
    io->ctx = sc;
    sc->io_map = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static void io_cleanup(acf_io_t *io, script_ctx_t *sc) {
    json_free(io->trace);
    json_free(io->relay_context);
    json_free(sc->cursors);
}

/* ── input preparation: materialize exc/failure/route specs ────────────── */
static json_t *prep_input(const char *func, json_t *in) {
    json_t *p = json_copy(in);
    json_t *v;
    v = json_obj_get(p, "exc");
    if (v && v->type == JSON_OBJECT) json_set(p, "exc", make_failure_or_exc(v));
    v = json_obj_get(p, "failure");
    if (v && v->type == JSON_OBJECT) json_set(p, "failure", make_failure(v));
    v = json_obj_get(p, "first_err");
    if (v && v->type == JSON_OBJECT) json_set(p, "first_err", make_failure_or_exc(v));
    v = json_obj_get(p, "error");
    if (v && v->type == JSON_OBJECT) json_set(p, "error", make_exc(v));
    v = json_obj_get(p, "fb_err");
    if (v && v->type == JSON_OBJECT) json_set(p, "fb_err", make_exc(v));
    v = json_obj_get(p, "judged");
    if (v && v->type == JSON_ARRAY) {
        json_t *arr = json_array();
        for (size_t i = 0; i < json_len(v); i++) json_append(arr, make_exc(json_get(v, i)));
        json_set(p, "judged", arr);
    }
    v = json_obj_get(p, "route");
    if (v && v->type == JSON_OBJECT) json_set(p, "route", normalize_route(v));
    v = json_obj_get(p, "context");
    if (v && v->type == JSON_OBJECT) json_set(p, "context", make_relay_context(v));
    (void)func;
    return p;
}
/* ── per-func dispatch ─────────────────────────────────────────────────── */
typedef json_t *(*acf_fn_t)(acf_io_t *, json_t *);

typedef struct {
    const char *func;
    acf_fn_t fn;
} acf_case_t;

static const acf_case_t ACF_CASES[] = {
    { "_field", acf_field },
    { "_message_field", acf_message_field },
    { "_contains_any", acf_contains_any },
    { "_creds_pair", acf_creds_pair },
    { "_creds_have_api_key", acf_creds_have_api_key },
    { "_coerce_positive_timeout", acf_coerce_positive_timeout },
    { "_without_temperature", acf_without_temperature },
    { "_without_max_tokens", acf_without_max_tokens },
    { "_without_reasoning_fields", acf_without_reasoning_fields },
    { "_affordable_max_tokens_from_error", acf_affordable_max_tokens_from_error },
    { "_elapsed_ms", acf_elapsed_ms },
    { "_stamp_latency_once", acf_stamp_latency_once },
    { "_record_route_info", acf_record_route_info },
    { "_fallback_lane", acf_fallback_lane },
    { "_coerce_llm_message", acf_coerce_llm_message },
    { "_dedupe_tool_names", acf_dedupe_tool_names },
    { "_tag_backend", acf_tag_backend },
    { "_provider_outcome", acf_provider_outcome },
    { "_ladder_step_call", acf_ladder_step_call },
    { "_failure_sender", acf_failure_sender },
    { "_is_structured_output_rejection", acf_is_structured_output_rejection },
    { "_is_reasoning_field_rejection", acf_is_reasoning_field_rejection },
    { "_is_reasoning_required_rejection", acf_is_reasoning_required_rejection },
    { "_is_max_tokens_rejection", acf_is_max_tokens_rejection },
    { "_param_rung_accepts", acf_param_rung_accepts },
    { "_task_prefers_fast_model", acf_task_prefers_fast_model },
    { "_fast_lane_config_fields", acf_fast_lane_config_fields },
    { "_compression_config_claims_fast_lane", acf_compression_config_claims_fast_lane },
    { "_compression_fast_lane_controls", acf_compression_fast_lane_controls },
    { "_get_task_no_progress_timeout", acf_get_task_no_progress_timeout },
    { "_unwrap_moa_provider", acf_unwrap_moa_provider },
    { "_preserve_provider_with_base_url", acf_preserve_provider_with_base_url },
    { "_builtin_provider_present", acf_builtin_provider_present },
    { "_profile_credentials_bypass_custom", acf_profile_credentials_bypass_custom },
    { "_named_custom_provider_present", acf_named_custom_provider_present },
    { "_routes_to_custom_endpoint", acf_routes_to_custom_endpoint },
    { "_classification_provider", acf_classification_provider },
    { "_provider_for_host", acf_provider_for_host },
    { "_relay_path", acf_relay_path },
    { "_context_too_small", acf_context_too_small },
    { "_nous_on_messages_wire", acf_nous_on_messages_wire },
    { "_is_gemini_native_route", acf_is_gemini_native_route },
    { "_forwards_max_tokens", acf_forwards_max_tokens },
    { "_stream_request_plan", acf_stream_request_plan },
    { "_with_custom_endpoint_extra_body", acf_with_custom_endpoint_extra_body },
    { "_merge_aux_extra_body", acf_merge_aux_extra_body },
    { "_unwrap_data_envelope", acf_unwrap_data_envelope },
    { "_note_relay_auxiliary_error", acf_note_relay_auxiliary_error },
    { "_should_skip_same_provider_retry", acf_should_skip_same_provider_retry },
    { "_should_retry_same_provider", acf_should_retry_same_provider },
    { "_outcome", acf_outcome },
    { "_aoutcome", acf_aoutcome },
    { "_parameter_rungs", acf_parameter_rungs },
    { "_classify_failure", acf_classify_failure },
    { "_failure", acf_failure },
    { "_rung", acf_rung },
    { "_ladder_parameter_rungs", acf_ladder_parameter_rungs },
    { "_refreshed_nous_step", acf_refreshed_nous_step },
    { "_ladder_nous_rungs", acf_ladder_nous_rungs },
    { "_failure_pool", acf_failure_pool },
    { "_recover_failure_pool", acf_recover_failure_pool },
    { "_ladder_credential_rungs", acf_ladder_credential_rungs },
    { "_next_fallback_after_quarantine", acf_next_fallback_after_quarantine },
    { "_ladder_provider_fallback", acf_ladder_provider_fallback },
    { "_aux_recovery_ladder", acf_aux_recovery_ladder },
    { "_drive_ladder", acf_drive_ladder },
    { "_primary_failure", acf_primary_failure },
    { "_start_recovery_ladder", acf_start_recovery_ladder },
    { "_prepare_aux_request", acf_prepare_aux_request },
    { "_plan_aux_call", acf_plan_aux_call },
    { "_fallback_request_kwargs", acf_fallback_request_kwargs },
    { "_plan_fallback_candidate", acf_plan_fallback_candidate },
    { "_quarantine_fallback_candidate", acf_quarantine_fallback_candidate },
    { "_plan_fallback_auth_retry", acf_plan_fallback_auth_retry },
    { "_fallback_candidate_ladder", acf_fallback_candidate_ladder },
    { "_candidate_classification_provider", acf_candidate_classification_provider },
};
#define ACF_CASE_COUNT (sizeof(ACF_CASES) / sizeof(ACF_CASES[0]))

/* ── line reader ───────────────────────────────────────────────────────── */
static char *read_line(FILE *f) {
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

        const acf_case_t *tc = NULL;
        for (size_t i = 0; i < ACF_CASE_COUNT; i++) {
            if (strcmp(ACF_CASES[i].func, func) == 0) { tc = &ACF_CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set(out, "error", json_string("unknown func"));
        } else {
            acf_io_t io;
            script_ctx_t sc;
            io_init(&io, &sc, in);
            json_t *pin = prep_input(func, in);
            json_t *r = tc->fn(&io, pin);
            json_free(pin);
            /* _drive_ladder returns either a raise descriptor or {"ret": final} */
            if (strcmp(func, "_drive_ladder") == 0 && r && !acf_raised(r)) {
                json_t *inner = json_obj_get(r, "ret");
                json_t *copy = inner ? json_copy(inner) : json_null();
                json_free(r);
                r = copy;
            }
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(io.trace));
            json_set(out, "relay_context", io.relay_context ? json_copy(io.relay_context) : json_null());
            io_cleanup(&io, &sc);
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
