/* t_port_auxiliary_client_flow3.c — harness for the UNCLAIMED-remainder FLOW
 * port (src/agent/auxiliary_client_flow3.c).
 *
 * stdin JSONL: {"func": <name>, "in": {...args..., "self":{...}, "io":{seam:script}}}.
 * stdout JSONL: {"func","in","out":{"ret","trace","state"}}.
 *   "ret"    canonical return (None -> null, raise -> {"__raise__":cls});
 *   "trace"  ordered seam events [{"ev","args","ret"}];
 *   "state"  post-call projection of the `self` carrier (events/lists/maps),
 *            or {} for the stateless leaves.
 * The oracle drives LIVE agent/auxiliary_client.py under the same scripts and
 * diffs out. Input is deep-copied before dispatch so the echoed "in" is pristine.
 */
#include "hermes_json.h"
#include "agent/auxiliary_client_flow3.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *ac3_harness_run(ac3_ctx *ctx, const char *func, json_t *self, json_t *in) {
    if (strcmp(func, "effective_deadline") == 0)
        return ac3_effective_deadline(ctx, self);
    if (strcmp(func, "cancel_requested") == 0)
        return ac3_cancel_requested(ctx, self);
    if (strcmp(func, "adopt_stream") == 0)
        return ac3_adopt_stream(ctx, self, json_obj_get(in, "stream"));
    if (strcmp(func, "release_stream") == 0)
        return ac3_release_stream(ctx, self, json_obj_get(in, "stream"));
    if (strcmp(func, "close_attempt_stream") == 0)
        return ac3_close_attempt_stream(ctx, self, json_get_str(in, "failure_note", ""));
    if (strcmp(func, "record_progress") == 0)
        return ac3_record_progress(ctx, self);
    if (strcmp(func, "timeout_message") == 0)
        return ac3_timeout_message(ctx, self);
    if (strcmp(func, "_close_client_on_timeout") == 0)
        return ac3_close_client_on_timeout(ctx, self);
    if (strcmp(func, "check_cancelled") == 0)
        return ac3_check_cancelled(ctx, self);
    if (strcmp(func, "_watchdog_fire") == 0)
        return ac3_watchdog_fire(ctx, self);
    if (strcmp(func, "_arm_timer") == 0)
        return ac3_arm_timer(ctx, self, json_get_num(in, "delay", 0));
    if (strcmp(func, "start") == 0)
        return ac3_start(ctx, self);
    if (strcmp(func, "on_event") == 0)
        return ac3_on_event(ctx, self, json_obj_get(in, "event"));
    if (strcmp(func, "_check_deadlines") == 0)
        return ac3_check_deadlines(ctx, self);
    if (strcmp(func, "_feed_reasoning_details") == 0)
        return ac3_feed_reasoning_details(ctx, self, json_obj_get(in, "delta"));
    if (strcmp(func, "_feed_tool_calls") == 0)
        return ac3_feed_tool_calls(ctx, self, json_obj_get(in, "delta"));
    if (strcmp(func, "__getattr__") == 0)
        return ac3_getattr(ctx, self, json_get_str(in, "name", ""));
    if (strcmp(func, "remember_temperature_rejection") == 0)
        return ac3_remember_temperature_rejection(ctx, self,
                   json_get_str(in, "provider", ""), json_get_str(in, "base_url", ""),
                   json_obj_get(json_obj_get(in, "rejected_kwargs"), "model"));
    if (strcmp(func, "_get_provider_chain") == 0)
        return ac3_get_provider_chain(ctx);
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
        json_t *in_orig = json_obj_get(rec, "in");
        if (!in_orig) in_orig = json_object();
        json_t *in = json_copy(in_orig);          /* working copy (mutated) */

        json_t *self = json_obj_get(in, "self");
        if (!self || self->type != JSON_OBJECT) {
            self = json_object();
            json_set(in, "self", self);
        }

        ac3_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;

        json_t *ret = ac3_harness_run(&ctx, func, self, in);
        json_t *ret_out;
        json_t *state = NULL;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(ac3_exc_class(&ctx)));
            json_free(ctx.exc);
            json_free(ret);
        } else {
            ret_out = ret ? ret : json_null();
            /* Echo the post-call self carrier as state. */
            state = json_copy(self);
        }

        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", state ? state : json_object());
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in_orig));
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) { fputs(s, stdout); fputc('\n', stdout); free(s); }
        json_free(emit);
        json_free(ctx.counters);
        json_free(in);
        json_free(rec);
    }
    return 0;
}
