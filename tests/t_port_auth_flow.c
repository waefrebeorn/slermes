/*
 * t_port_auth_flow.c — oracle harness for the UNCLAIMED auth-surface FLOW
 * cores of hermes_cli/auth.py ported in src/cli/auth_flow.c.
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/auth_flow.jsonl), wires the universal seam
 * to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}} (plus "state" for
 * the two mutation flows: store_section, quarantine_flat_oauth_state).
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty array -> None). tests/oracle_auth_flow.py drives the
 * LIVE Python with the same scripts and compares the ordered decision
 * sequence + return value (raise envelopes included).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/auth_flow.c"

typedef struct {
    const json_t *in;
    au_trace_t *trace;
    json_t *counters;
} au_script_ctx_t;

static json_t *au_args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(out, args->c.keys[i], json_copy(v));
        }
    }
    return out;
}

static json_t *au_script_call(void *vctx, const char *name, const json_t *args) {
    au_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        /* fail-closed: an unscripted seam raises RuntimeError on both sides */
        char *msg = au_xsf("unscripted seam: %s", name);
        result = au_raise_spec("RuntimeError", msg);
        free(msg);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", au_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    au_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

static void run_case(const char *func, const json_t *in, json_t *out) {
    au_trace_t trace = {NULL};
    json_t *counters = json_object();
    au_script_ctx_t ctx = {in, &trace, counters};
    au_seams_t seams = {&ctx, au_script_call};
    const au_seams_t *sp = &seams;
    json_t *ret = NULL;
    json_t *tmpnode = NULL;   /* defaulted input node owned by the harness */
    json_t *tmpnode2 = NULL;
    json_t *workstate = NULL; /* mutation-flow working copy */
    bool echo_state = false;

    /* shared defaulted inputs */
    const json_t *registry = J("registry");
    if (!registry) { tmpnode = json_object(); registry = tmpnode; }
    const json_t *keyenv = J("keyenv");
    if (!keyenv) {
        tmpnode2 = json_object();
        json_set(tmpnode2, "__callable__", json_string("scoped_key_env"));
        keyenv = tmpnode2;
    }
    const char *os_name = json_get_str(in, "os_name", "nt");
    const char *backend = json_get_str(in, "lock_backend", "msvcrt");

    if (!strcmp(func, "api_key_provider")) {
        ret = au_api_key_provider(json_get_str(in, "id", ""), json_get_str(in, "name", ""),
                                  json_get_str(in, "inference_base_url", ""),
                                  J("api_key_env_vars"),
                                  json_get_str(in, "base_url_env_var", ""),
                                  json_get_str(in, "auth_type", "api_key"));
    } else if (!strcmp(func, "empty_auth_store")) {
        ret = au_empty_auth_store();
    } else if (!strcmp(func, "store_section")) {
        workstate = J("state") ? json_copy(J("state")) : json_object();
        ret = au_store_section(workstate, json_get_str(in, "key", ""));
        echo_state = true;
    } else if (!strcmp(func, "provider_state_in")) {
        ret = au_provider_state_in(J("store"), json_get_str(in, "provider_id", ""));
    } else if (!strcmp(func, "slot_selects")) {
        ret = au_slot_selects(J("slot"), json_get_str(in, "normalized", ""));
    } else if (!strcmp(func, "model_level_key_env")) {
        ret = au_model_level_key_env(sp, json_get_str(in, "provider_id", ""));
    } else if (!strcmp(func, "active_provider_is")) {
        ret = au_active_provider_is(sp, json_get_str(in, "normalized", ""));
    } else if (!strcmp(func, "config_selects_provider")) {
        ret = au_config_selects_provider(sp, json_get_str(in, "normalized", ""));
    } else if (!strcmp(func, "env_secret")) {
        ret = au_env_secret(sp, json_get_str(in, "name", ""));
    } else if (!strcmp(func, "explicit_env_credentials_present")) {
        ret = au_explicit_env_credentials_present(sp, registry, json_get_str(in, "normalized", ""));
    } else if (!strcmp(func, "keyless_provider_has_explicit_config")) {
        ret = au_keyless_provider_has_explicit_config(sp, json_get_str(in, "normalized", ""));
    } else if (!strcmp(func, "refuse_env_adoption_if_config_corrupt")) {
        ret = au_refuse_env_adoption_if_config_corrupt(sp);
    } else if (!strcmp(func, "plugin_aliases")) {
        ret = au_plugin_aliases(sp);
    } else if (!strcmp(func, "scoped_key_env_reader")) {
        ret = au_scoped_key_env_reader(sp);
    } else if (!strcmp(func, "openrouter_auto_detected")) {
        ret = au_openrouter_auto_detected(sp, keyenv);
    } else if (!strcmp(func, "logged_in_oauth_active_provider")) {
        ret = au_logged_in_oauth_active_provider(sp, registry,
                                                 json_get_bool(in, "skip_free_tier", false));
    } else if (!strcmp(func, "config_model_provider")) {
        ret = au_config_model_provider(sp, registry);
    } else if (!strcmp(func, "credential_profile")) {
        ret = au_credential_profile(sp, json_get_str(in, "provider_id", ""));
    } else if (!strcmp(func, "env_key_auto_detected")) {
        ret = au_env_key_auto_detected(sp, keyenv, S("oauth_active"));
    } else if (!strcmp(func, "pool_entry_is_explicit")) {
        ret = au_pool_entry_is_explicit(sp, J("entry"));
    } else if (!strcmp(func, "explicit_pool_entry_present")) {
        ret = au_explicit_pool_entry_present(sp, json_get_str(in, "normalized", ""));
    } else if (!strcmp(func, "is_lock_contention")) {
        ret = au_is_lock_contention(J("exc"));
    } else if (!strcmp(func, "lock_holder_hint")) {
        ret = au_lock_holder_hint(sp, json_get_str(in, "path", ""), os_name);
    } else if (!strcmp(func, "kernel_lock")) {
        ret = au_kernel_lock(sp, backend, json_get_bool(in, "acquire", true));
    } else if (!strcmp(func, "stamp_lock_holder_pid")) {
        ret = au_stamp_lock_holder_pid(sp);
    } else if (!strcmp(func, "clear_stamped_lock_holder_pid")) {
        ret = au_clear_stamped_lock_holder_pid(sp, backend);
    } else if (!strcmp(func, "tls_state_from_verify")) {
        ret = au_tls_state_from_verify(J("verify"));
    } else if (!strcmp(func, "utc_now_z")) {
        ret = au_utc_now_z(sp);
    } else if (!strcmp(func, "last_auth_error_marker")) {
        ret = au_last_auth_error_marker(sp, json_get_str(in, "provider", ""), J("error"),
                                        json_get_str(in, "reason", ""), J("default_code"));
    } else if (!strcmp(func, "quarantine_flat_oauth_state")) {
        workstate = J("state") ? json_copy(J("state")) : json_object();
        ret = au_quarantine_flat_oauth_state(sp, workstate, json_get_str(in, "provider", ""),
                                             J("error"));
        echo_state = true;
    } else if (!strcmp(func, "is_terminal_refresh_error")) {
        ret = au_is_terminal_refresh_error(J("exc"), json_get_str(in, "provider", ""));
    } else if (!strcmp(func, "codex_pool_rate_limited_status")) {
        ret = au_codex_pool_rate_limited_status(sp);
    } else if (!strcmp(func, "resolved_key")) {
        ret = au_resolved_key(sp, json_get_str(in, "path", ""));
    } else if (!strcmp(func, "save_private_json")) {
        ret = au_save_private_json(sp, json_get_str(in, "target", ""), J("data"),
                                   json_get_bool(in, "fsync_dir", false), J("dump_kwargs"));
    } else if (!strcmp(func, "save_active_provider_state")) {
        ret = au_save_active_provider_state(sp, json_get_str(in, "provider_id", ""), J("state"));
    } else if (!strcmp(func, "suppressed_credential_sources")) {
        ret = au_suppressed_credential_sources(sp, json_get_str(in, "provider_id", ""));
    } else if (!strcmp(func, "get_aws_sdk_auth_status")) {
        ret = au_get_aws_sdk_auth_status(sp, json_get_str(in, "target", ""));
    } else if (!strcmp(func, "external_process_spec")) {
        ret = au_external_process_spec(sp, J("pconfig"));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    if (echo_state) json_set(out, "state", workstate ? json_copy(workstate) : json_object());
    au_trace_free(&trace);
    json_free(counters);
    if (tmpnode) json_free(tmpnode);
    if (tmpnode2) json_free(tmpnode2);
    if (workstate) json_free(workstate);
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) > 0) {
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case: %s\n", err ? err : "not object");
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
        char *func_s = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    free(line);
    return 0;
}