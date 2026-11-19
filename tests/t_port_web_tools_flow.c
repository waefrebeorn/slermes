/* t_port_web_tools_flow.c — harness for the tools/web_tools.py FLOW port
 * (src/tools/web_tools_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}} where "in" carries
 * the call arguments plus "io" (seam-name -> script: single value, {"$seq":[...]}
 * per-call queue, or {"__raise__":cls,"msg":m,...}; a MISSING key fail-closes).
 * Emits JSON LINES:
 *   {"func", "in", "out": {"ret", "trace", "state"}}
 * "ret" is the canonical return (tuple -> array, None -> null, uncaught
 * exception -> {"__raise__":cls}); "trace" is the ordered seam-call sequence
 * [{"ev","args","ret"}]; "state" is the post-call mutation window
 * ({"debug_call_data":...} for _finish_debug, {} otherwise) — echoed on RAISE too.
 * The oracle drives LIVE quarry code under the SAME scripts and diffs out.
 * Dispatch runs on a deep COPY of "in" so the emitted "in" stays pristine.
 */
#include "hermes_json.h"
#include "tools/web_tools_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *wt_state_win(const char *key, json_t *node) {
    json_t *o = json_object();
    json_set(o, key, node ? json_copy(node) : json_null());
    return o;
}

static json_t *wt_harness_run(wt_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_configured_backend") == 0)
        return wt_configured_backend(ctx, json_get_str(in, "key", "backend"));
    if (strcmp(func, "_registry_call") == 0)
        return wt_registry_call(ctx, json_get_str(in, "func_name", ""),
                                json_obj_get(in, "default"), json_obj_get(in, "args"));
    if (strcmp(func, "_probe") == 0)
        return wt_probe(ctx, json_obj_get(in, "provider"), json_get_str(in, "method", "is_available"));
    if (strcmp(func, "_autodetect_backend") == 0)
        return wt_autodetect_backend(ctx);
    if (strcmp(func, "_keyless_backend") == 0)
        return wt_keyless_backend(ctx);
    if (strcmp(func, "_managed_web_search") == 0)
        return wt_managed_web_search(ctx);
    if (strcmp(func, "_xai_available") == 0)
        return wt_xai_available(ctx);
    if (strcmp(func, "_finish_debug") == 0) {
        json_t *dcd = json_obj_get(in, "debug_call_data");
        json_t *r = wt_finish_debug(ctx, json_get_str(in, "call_name", ""),
                                    dcd, json_obj_get(in, "error_msg"));
        *state_out = wt_state_win("debug_call_data", dcd);
        return r;
    }
    if (strcmp(func, "_memoized_search") == 0)
        return wt_memoized_search(ctx, json_obj_get(in, "provider"),
                                  json_get_str(in, "query", ""), json_get_num(in, "limit", 5));
    if (strcmp(func, "_ring_vendor_keyless") == 0)
        return wt_ring_vendor_keyless(ctx, json_get_str(in, "name", ""));
    if (strcmp(func, "_managed_search_fallback") == 0)
        return wt_managed_search_fallback(ctx, json_obj_get(in, "provider"),
                                          json_get_str(in, "original_error", ""),
                                          json_get_str(in, "query", ""), json_get_num(in, "limit", 5));
    if (strcmp(func, "_rescue_search") == 0)
        return wt_rescue_search(ctx, json_get_str(in, "provider_name", ""),
                                json_get_str(in, "original_error", ""),
                                json_get_str(in, "query", ""), json_get_num(in, "limit", 5));
    if (strcmp(func, "_rescue_extract") == 0)
        return wt_rescue_extract(ctx, json_get_str(in, "provider_name", ""),
                                 json_obj_get(in, "urls"), json_obj_get(in, "results"));
    if (strcmp(func, "_clamp_char_limit") == 0)
        return wt_clamp_char_limit(ctx, json_obj_get(in, "value"));
    if (strcmp(func, "_clamp_or_default") == 0)
        return wt_clamp_or_default(ctx, json_obj_get(in, "value"));
    if (strcmp(func, "_effective_char_limit") == 0)
        return wt_effective_char_limit(ctx, json_obj_get(in, "char_limit"));
    if (strcmp(func, "_trim_results") == 0)
        return wt_trim_results(json_obj_get(in, "results"));
    if (strcmp(func, "_disabled_plugin_error") == 0)
        return wt_disabled_plugin_error(json_get_str(in, "capability", ""), json_get_str(in, "disabled_key", ""));
    if (strcmp(func, "_no_provider_error") == 0)
        return wt_no_provider_error(ctx, json_get_str(in, "capability", ""), json_get_str(in, "fallback", ""));
    if (strcmp(func, "_strict_selection_error") == 0)
        return wt_strict_selection_error(ctx, json_get_str(in, "capability", ""), json_get_str(in, "backend", ""));
    if (strcmp(func, "_result_entry") == 0)
        return wt_result_entry(json_get_str(in, "url", ""), json_obj_get(in, "error"));
    if (strcmp(func, "_extract_error_json") == 0)
        return wt_extract_error_json(json_get_str(in, "error", ""));
    if (strcmp(func, "_refuse_all") == 0)
        return wt_refuse_all(json_get_str(in, "error", ""));
    if (strcmp(func, "_merge_in_order") == 0)
        return wt_merge_in_order(json_obj_get(in, "fixed"), json_get_num(in, "total", 0),
                                 json_obj_get(in, "fetch_positions"), json_obj_get(in, "fetch_urls"),
                                 json_obj_get(in, "results"));
    if (strcmp(func, "_resolve_extract_provider") == 0)
        return wt_resolve_extract_provider(ctx, json_get_str(in, "backend", ""));
    if (strcmp(func, "_extract_timeout_seconds") == 0)
        return wt_extract_timeout_seconds(ctx);
    if (strcmp(func, "_backend_failed_whole_batch") == 0)
        return wt_backend_failed_whole_batch(ctx, json_obj_get(in, "results"));
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 16];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) {
            fprintf(stderr, "harness: unparseable case line\n");
            return 1;
        }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *empty_in = NULL;
        if (!in) { empty_in = json_object(); in = empty_in; }
        json_t *work = json_copy(in);   /* dispatch mutates the copy, not "in" */
        wt_ctx ctx;
        ctx.io = json_obj_get(work, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = wt_harness_run(&ctx, func, work, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_copy(json_obj_get(ctx.exc, "__raise__")));
            json_free(ctx.exc);
            if (ret) json_free(ret);
        } else {
            ret_out = ret ? ret : json_null();
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", state ? state : json_object());
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in));
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) {
            fputs(s, stdout);
            fputc('\n', stdout);
            free(s);
        }
        json_free(emit);
        json_free(ctx.counters);
        json_free(work);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}