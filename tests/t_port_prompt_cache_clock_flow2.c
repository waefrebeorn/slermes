/* t_port_prompt_cache_clock_flow2.c — harness for the UNCLAIMED FLOW remainder
 * port (src/agent/prompt_cache_clock_flow2.c).
 *
 * Reads JSON lines {"func":<name>,"in":{...}}. "in" carries call arguments plus
 * "io" (seam-name -> script) and "env" (scripted constants/flags). Emits
 * {"func","in","out":{"ret","trace","state"}}. "trace" is the ordered seam-name
 * sequence; the oracle drives LIVE agent/prompt_cache_clock.py under the same
 * scripts and diffs ret + trace + state.
 */
#include "hermes_json.h"
#include "agent/prompt_cache_clock_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *h_run(pcc2_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    json_t *agent, *usage, *attempt, *witness, *upstream, *event, *window;

    if (!strcmp(func, "_list_field"))
        return pcc2_list_field(json_obj_get(in, "api_kwargs"), json_get_str(in, "key", ""));
    if (!strcmp(func, "record_attempt_sent")) {
        agent = json_obj_get(in, "agent");
        pcc2_record_attempt_sent(ctx, agent, json_obj_get(in, "api_kwargs"));
        *state_out = agent; return NULL;
    }
    if (!strcmp(func, "_refresh_restart"))
        return json_number(pcc2_refresh_restart(ctx, json_get_num(in, "at", 0), json_get_str(in, "tier", "")));
    if (!strcmp(func, "_refresh_openai"))
        return json_number(pcc2_refresh_openai(json_get_num(in, "deadline", 0), json_get_num(in, "at", 0)));
    if (!strcmp(func, "_managed_unload_end"))
        return pcc2_managed_unload_end(ctx, json_get_str(in, "base_url", ""), json_get_num(in, "idle_from", 0));
    if (!strcmp(func, "_route_key"))
        return pcc2_route_key(ctx, json_obj_get(in, "provider"), json_obj_get(in, "base_url"),
                              json_obj_get(in, "model"));
    if (!strcmp(func, "response_upstream"))
        return pcc2_response_upstream(json_obj_get(in, "response"));
    if (!strcmp(func, "upstream_switched"))
        return pcc2_upstream_switched(json_obj_get(in, "before"), json_obj_get(in, "after"));
    if (!strcmp(func, "_display_route"))
        return pcc2_display_route(ctx, json_obj_get(in, "agent"));
    if (!strcmp(func, "_compression_count"))
        return json_number((double)pcc2_compression_count(json_obj_get(in, "agent")));
    if (!strcmp(func, "_storage_errors"))
        return pcc2_storage_errors();
    if (!strcmp(func, "_clock_store"))
        return pcc2_clock_store(ctx, json_obj_get(in, "agent"));
    if (!strcmp(func, "_stored_clock"))
        return pcc2_stored_clock(ctx, json_obj_get(in, "store"));
    if (!strcmp(func, "_agent_generation"))
        return pcc2_agent_generation(ctx, json_obj_get(in, "agent"));
    if (!strcmp(func, "_agent_invalid_reason"))
        return pcc2_agent_invalid_reason(ctx, json_obj_get(in, "agent"), json_obj_get(in, "event"),
                                         json_obj_get(in, "generation"));
    if (!strcmp(func, "context_witness"))
        return pcc2_context_witness(ctx, json_obj_get(in, "agent"));
    if (!strcmp(func, "_set_event")) {
        agent = json_obj_get(in, "agent");
        pcc2_set_event(ctx, agent, json_obj_get(in, "event"), json_obj_get(in, "mark"));
        *state_out = agent; return NULL;
    }
    if (!strcmp(func, "_moa_destination"))
        return pcc2_moa_destination(json_obj_get(in, "agent"));
    if (!strcmp(func, "_learn")) {
        pcc2_learn(ctx, json_obj_get(in, "previous"), json_get_str(in, "provider", ""),
                   json_get_str(in, "base_url", ""), json_get_str(in, "model", ""),
                   json_obj_get(in, "attempt"), (long long)json_get_num(in, "read", 0),
                   (long long)json_get_num(in, "write", 0), json_get_num(in, "at", 0),
                   json_obj_get(in, "upstream"), json_get_str(in, "session_id", ""));
        return NULL;
    }
    if (!strcmp(func, "_declared_tier"))
        return pcc2_declared_tier(ctx, json_get_str(in, "route_class", ""), json_obj_get(in, "requested_ttl"),
                                  (long long)json_get_num(in, "write_tokens", 0),
                                  json_get_str(in, "provider", ""), json_get_str(in, "base_url", ""));
    if (!strcmp(func, "declared_write_tier"))
        return pcc2_declared_write_tier(ctx, json_obj_get(in, "event"));
    if (!strcmp(func, "request_write_tier"))
        return pcc2_request_write_tier(ctx, json_obj_get(in, "api_kwargs"), json_obj_get(in, "usage"),
                                       json_get_str(in, "provider", ""), json_get_str(in, "base_url", ""),
                                       json_get_str(in, "api_mode", ""), json_get_str(in, "model", ""));
    if (!strcmp(func, "_learn_silent_miss")) {
        pcc2_learn_silent_miss(ctx, json_obj_get(in, "agent"), json_obj_get(in, "usage"),
                               json_get_str(in, "model", ""), json_get_str(in, "provider", ""),
                               json_get_str(in, "base_url", ""), json_obj_get(in, "attempt"),
                               json_obj_get(in, "upstream"));
        *state_out = json_obj_get(in, "agent"); return NULL;
    }
    if (!strcmp(func, "record_cache_response")) {
        *state_out = json_obj_get(in, "agent");
        return pcc2_record_cache_response(ctx, json_obj_get(in, "agent"), json_obj_get(in, "usage"),
                json_get_str(in, "model", ""), json_get_str(in, "provider", ""),
                json_get_str(in, "base_url", ""), json_get_str(in, "api_mode", ""),
                json_obj_get(in, "attempt"), json_obj_get(in, "witness"), json_obj_get(in, "upstream"));
    }
    if (!strcmp(func, "record_unobserved_cache_response")) {
        *state_out = json_obj_get(in, "agent");
        return pcc2_record_unobserved(ctx, json_obj_get(in, "agent"), json_obj_get(in, "usage"),
                                      json_get_str(in, "reason", ""));
    }
    if (!strcmp(func, "event_to_json"))
        return pcc2_event_to_json(json_obj_get(in, "event"));
    if (!strcmp(func, "_fallback_event"))
        return pcc2_fallback_event(ctx, json_obj_get(in, "agent"), json_get_num(in, "last_assistant_at", 0),
                                   json_obj_get(in, "generation"));
    if (!strcmp(func, "restore_cache_clock")) {
        *state_out = json_obj_get(in, "agent");
        return pcc2_restore_cache_clock(ctx, json_obj_get(in, "agent"));
    }
    if (!strcmp(func, "_unknown"))
        return pcc2_unknown(json_get_str(in, "reason", ""), json_obj_get(in, "event"));
    if (!strcmp(func, "_window"))
        return pcc2_window(json_obj_get(in, "event"));
    if (!strcmp(func, "cache_window")) {
        *state_out = json_obj_get(in, "agent");
        return pcc2_cache_window(ctx, json_obj_get(in, "agent"));
    }
    if (!strcmp(func, "_saved_route_changed"))
        return pcc2_saved_route_changed(ctx, json_obj_get(in, "event"), json_obj_get(in, "saved"));
    if (!strcmp(func, "stored_cache_window"))
        return pcc2_stored_cache_window(ctx, json_obj_get(in, "raw_clock"), json_get_str(in, "session_id", ""),
                                        json_obj_get(in, "generation"), json_obj_get(in, "saved_route"));
    if (!strcmp(func, "cache_window_payload"))
        return pcc2_cache_window_payload(ctx, json_obj_get(in, "agent"));
    if (!strcmp(func, "_field"))
        return pcc2_field(json_obj_get(in, "window"), json_get_str(in, "name", ""));
    if (!strcmp(func, "_clock"))
        return pcc2_clock(ctx, json_get_num(in, "ts", 0));
    if (!strcmp(func, "_cache_line"))
        return pcc2_cache_line(ctx, json_obj_get(in, "window"), json_obj_get(in, "estimated"),
                               json_obj_get(in, "now"));
    if (!strcmp(func, "render_prompt_cache_line"))
        return pcc2_render_line(ctx, json_obj_get(in, "window"), json_obj_get(in, "now"));
    if (!strcmp(func, "prompt_cache_is_cold"))
        return pcc2_prompt_cache_is_cold(ctx, json_obj_get(in, "agent"), json_obj_get(in, "now"));
    if (!strcmp(func, "_valid_previous"))
        return pcc2_valid_previous(ctx, json_obj_get(in, "agent"), json_obj_get(in, "generation"));
    (void)agent; (void)usage; (void)attempt; (void)witness; (void)upstream;
    (void)event; (void)window;
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 16];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) { fprintf(stderr, "harness: unparseable case line\n"); return 1; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *empty_in = NULL;
        if (!in) { empty_in = json_object(); in = empty_in; }
        /* Snapshot BEFORE h_run: mutation ports (record_cache_response et al.)
         * modify `in.agent` in place; the emitted "in" must be the original
         * case, or the oracle re-drives an already-mutated agent. */
        json_t *in_snapshot = json_copy(in);
        pcc2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        ctx.state = NULL;
        json_t *state = NULL;
        json_t *ret = h_run(&ctx, func, in, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_copy(json_obj_get(ctx.exc, "__raise__")));
            json_free(ctx.exc);
            json_free(ret);
        } else {
            /* ret may be borrowed from the input record (field/getattr leaves);
             * copy before transferring into out, else rec free double-frees it. */
            ret_out = ret ? json_copy(ret) : json_null();
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", state ? json_copy(state) : json_object());
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", in_snapshot);
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) { fputs(s, stdout); fputc('\n', stdout); free(s); }
        json_free(emit);
        json_free(ctx.counters);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}
