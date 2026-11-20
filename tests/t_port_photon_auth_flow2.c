/* t_port_photon_auth_flow2.c — harness for the Photon auth FLOW port
 * (src/gateway/platforms/photon_auth_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args","state","io"}}.
 * Emits JSON LINES: {"func","in","out"} where out captures the ordered seam
 * events ("trace"), the final return ("ret") and the state echo ("state",
 * always {} for these module functions).  The echoed "in" is the ORIGINAL
 * input; the oracle re-drives live Python from it.
 *
 * Scripted I/O lives in in.io: map seam-name -> result (plain value, a LIST
 * consumed one element per call, or {"__script__":true,"ret"/"raise":...}).
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/photon_auth_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*pa2_fn_t)(pa2_ctx_t *, const json_t *);
typedef struct { const char *func; pa2_fn_t fn; } pa2_case_t;

/* Adapters: a few ported helpers take (ctx, value, extra); the universal
 * harness signature is (ctx, args). The extra values come from case args. */
static json_t *pa2_w_header_value(pa2_ctx_t *c, const json_t *a) {
    return pa2__header_value(c, json_obj_get(a, "headers"), json_get_str(a, "name", ""));
}
static json_t *pa2_w_raise_on_error_key(pa2_ctx_t *c, const json_t *a) {
    return pa2__raise_on_error_key(c, json_obj_get(a, "data"), json_get_str(a, "action", ""));
}
static json_t *pa2_w_raise_for_status(pa2_ctx_t *c, const json_t *a) {
    return pa2__raise_for_status(c, json_obj_get(a, "resp"), json_get_str(a, "action", ""));
}
static json_t *pa2_w_pool_first(pa2_ctx_t *c, const json_t *a) {
    return pa2__pool_first(c, json_obj_get(a, "auth"), json_get_str(a, "key", ""));
}
static json_t *pa2_w_store_pool_record(pa2_ctx_t *c, const json_t *a) {
    return pa2__store_pool_record(c, json_get_str(a, "key", ""), json_obj_get(a, "record"));
}
static json_t *pa2_w_device_candidates(pa2_ctx_t *c, const json_t *a) {
    return pa2__device_response_token_candidates(c, json_obj_get(a, "body"),
                                                  json_obj_get(a, "headers"));
}
static json_t *pa2_w_bearer(pa2_ctx_t *c, const json_t *a) {
    return pa2__bearer(c, json_obj_get(a, "token"));
}
static json_t *pa2_w_clean_token(pa2_ctx_t *c, const json_t *a) {
    return pa2__clean_bearer_token(c, json_obj_get(a, "value"));
}
static json_t *pa2_w_normalize_phone(pa2_ctx_t *c, const json_t *a) {
    return pa2__normalize_phone(c, json_obj_get(a, "phone"));
}
static json_t *pa2_w_unwrap_list(pa2_ctx_t *c, const json_t *a) {
    return pa2__unwrap_list(c, json_obj_get(a, "data"));
}
static json_t *pa2_w_user_assigned(pa2_ctx_t *c, const json_t *a) {
    return pa2_user_assigned_line(c, json_obj_get(a, "user"));
}
static json_t *pa2_w_user_opted(pa2_ctx_t *c, const json_t *a) {
    return pa2_user_opted_in(c, json_obj_get(a, "user"));
}
static json_t *pa2_w_save_auth(pa2_ctx_t *c, const json_t *a) {
    return pa2__save_auth(c, json_obj_get(a, "data"));
}
static json_t *pa2_w_response_detail(pa2_ctx_t *c, const json_t *a) {
    return pa2__response_error_detail(c, json_obj_get(a, "resp"));
}
static json_t *pa2_w_validated(pa2_ctx_t *c, const json_t *a) {
    return pa2__validated_dashboard_token(c, json_obj_get(a, "candidates"));
}

static const pa2_case_t CASES[] = {
    { "_bearer", pa2_w_bearer },
    { "_basic", pa2__basic },
    { "_require_httpx", pa2__require_httpx },
    { "_dashboard_host", pa2__dashboard_host },
    { "_spectrum_host", pa2__spectrum_host },
    { "_clean_bearer_token", pa2_w_clean_token },
    { "_header_value", pa2_w_header_value },
    { "_normalize_phone", pa2_w_normalize_phone },
    { "_raise_on_error_key", pa2_w_raise_on_error_key },
    { "_unwrap_list", pa2_w_unwrap_list },
    { "user_assigned_line", pa2_w_user_assigned },
    { "user_opted_in", pa2_w_user_opted },
    { "_get_config_env_value", pa2__get_config_env_value },
    { "_configured_operator_phone", pa2__configured_operator_phone },
    { "_safe", pa2__safe },
    { "_response_error_detail", pa2_w_response_detail },
    { "_raise_for_status", pa2_w_raise_for_status },
    { "_pool_first", pa2_w_pool_first },
    { "_auth_json_path", pa2__auth_json_path },
    { "_load_auth", pa2__load_auth },
    { "_save_auth", pa2_w_save_auth },
    { "_store_pool_record", pa2_w_store_pool_record },
    { "load_photon_token", pa2_load_photon_token },
    { "store_photon_token", pa2_store_photon_token },
    { "clear_photon_token", pa2_clear_photon_token },
    { "load_project_credentials", pa2_load_project_credentials },
    { "load_dashboard_project_id", pa2_load_dashboard_project_id },
    { "store_project_credentials", pa2_store_project_credentials },
    { "store_user_numbers", pa2_store_user_numbers },
    { "_persist_runtime_env", pa2__persist_runtime_env },
    { "_device_response_token_candidates", pa2_w_device_candidates },
    { "request_device_code", pa2_request_device_code },
    { "check_photon_token_valid", pa2_check_photon_token_valid },
    { "validate_photon_token", pa2_validate_photon_token },
    { "_validated_dashboard_token", pa2_w_validated },
    { "_dashboard_get", pa2__dashboard_get },
    { "_dashboard_post", pa2__dashboard_post },
    { "_dashboard_list", pa2__dashboard_list },
    { "list_projects", pa2_list_projects },
    { "find_project_by_name", pa2_find_project_by_name },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)ch;
    }
    if (ch == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    json_t *empty_args = json_object();
    char *line;
    while ((line = read_line(stdin)) != NULL) {
        size_t llen = strlen(line);
        while (llen && (line[llen - 1] == '\r' || line[llen - 1] == '\n')) line[--llen] = '\0';
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) in = NULL;
        json_t *out = json_object();

        const pa2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (!strcmp(CASES[i].func, func)) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            pa2_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", ctx.state);
            json_free(ctx.cursors);
        }
        json_t *echo = json_object();
        json_set_hdr(echo, "func", func);
        json_set(echo, "in", in ? json_copy(in) : json_null());
        json_set(echo, "out", out);
        char *s = json_serialize(echo);
        if (s) { printf("%s\n", s); free(s); }
        json_free(echo);
        json_free(rec);
    }
    json_free(empty_args);
    return 0;
}
