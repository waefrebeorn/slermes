/* t_port_runtime_provider_flow2.c — fixture-driven harness for the
 * hermes_cli/runtime_provider.py UNCLAIMED-remainder FLOW port.
 *
 * Reads JSONL cases {"func":<name>,"in":{...,"io":{seam:script},...}} from stdin,
 * emits JSONL {"func","in","out":{"ret","trace","state"}}. The ported C emits only
 * what it did; the oracle (oracle_runtime_provider_flow2.py) drives the LIVE defs
 * under the same scripts and diffs. Self-contained: includes the port by bare name.
 */
#include "hermes_json.h"
#include "cli/runtime_provider_flow2.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *rp2h_run(rp2_ctx *ctx, const char *f, json_t *in) {
#define RP2H(F, C) if (strcmp(f, F) == 0) return C(ctx, in)
    RP2H("load_config", rp2_load_config);
    RP2H("get_compatible_custom_providers", rp2_get_compatible_custom_providers);
    RP2H("normalize_extra_headers", rp2_normalize_extra_headers);
    RP2H("_effective_model", rp2__effective_model);
    RP2H("_pool_entry_api_key", rp2__pool_entry_api_key);
    RP2H("_pool_entry_base_url", rp2__pool_entry_base_url);
    RP2H("_nous_min_key_ttl", rp2__nous_min_key_ttl);
    RP2H("_finalize_base_url", rp2__finalize_base_url);
    RP2H("_owned_url_credential", rp2__owned_url_credential);
    RP2H("_actual_local_key", rp2__actual_local_key);
    RP2H("_actual_url", rp2__actual_url);
    RP2H("_named_custom_rung", rp2__named_custom_rung);
    RP2H("_openrouter_fallback", rp2__openrouter_fallback);
#undef RP2H
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 18];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) { fprintf(stderr, "harness: unparseable case line\n"); return 1; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *empty = NULL;
        if (!in) { empty = json_object(); in = empty; }
        rp2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *ret = rp2h_run(&ctx, func, in);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(rp2_exc_class(&ctx)));
            json_set(ret_out, "msg", json_string(json_get_str(ctx.exc, "msg", "")));
            json_free(ctx.exc); json_free(ret);
        } else ret_out = ret ? ret : json_null();
        json_t *state = json_obj_get(in, "state");
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", state ? json_copy(state) : json_object());
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in));
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) { fputs(s, stdout); fputc('\n', stdout); free(s); }
        json_free(emit); json_free(ctx.counters); json_free(empty); json_free(rec);
    }
    return 0;
}
